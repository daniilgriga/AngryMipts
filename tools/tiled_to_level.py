#!/usr/bin/env python3
import argparse
import json
import math
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path


DEFAULT_STAR_THRESHOLDS = [1000, 2500, 4000]
DEFAULT_TOTAL_SHOTS = 4
DEFAULT_MAX_PULL = 120
TRIANGLE_MIN_TWICE_AREA = 1e-3
MATERIAL_BLOCK_BONUS = {
    "Wood": 50,
    "Stone": 100,
    "Glass": 20,
    "Ice": 30,
}
REPO_ROOT = Path(__file__).resolve().parent.parent


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert a Tiled .tmj level into AngryMipts gameplay JSON."
    )
    parser.add_argument("input", type=Path, help="Path to the source .tmj file")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Path to the generated gameplay JSON file",
    )
    return parser.parse_args()


def load_json(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def get_property_map(properties: list[dict] | None) -> dict[str, object]:
    result: dict[str, object] = {}
    for prop in properties or []:
        name = prop.get("name")
        if isinstance(name, str):
            result[name] = prop.get("value")
    return result


def resolve_template_path(map_path: Path, template_ref: str) -> Path:
    return (map_path.parent / template_ref).resolve()


def load_template_data(template_path: Path) -> dict[str, object]:
    root = ET.parse(template_path).getroot()
    obj = root.find("object")
    if obj is None:
        return {}

    template: dict[str, object] = {
        "type": obj.get("type", ""),
        "width": float(obj.get("width", "0") or 0),
        "height": float(obj.get("height", "0") or 0),
        "properties": {},
    }

    properties_node = obj.find("properties")
    if properties_node is not None:
        properties: dict[str, object] = {}
        for prop in properties_node.findall("property"):
            name = prop.get("name")
            if not name:
                continue
            prop_type = str(prop.get("type") or "").strip().lower()
            value = prop.get("value")
            if value is None and prop.text is not None:
                value = prop.text
            if prop_type == "bool" and value is not None:
                value = str(value).strip().lower() in {"1", "true", "yes", "on"}
            elif prop_type in {"int", "float"} and value is not None:
                numeric_text = str(value).strip()
                if prop_type == "int":
                    value = int(float(numeric_text))
                else:
                    value = float(numeric_text)
            properties[name] = value
        template["properties"] = properties

    return template


def normalize_material(value: object) -> str:
    normalized = str(value or "").strip().lower()
    mapping = {
        "wood": "Wood",
        "stone": "Stone",
        "glass": "Glass",
        "ice": "Ice",
    }
    if normalized not in mapping:
        raise ValueError(f"Unknown material value: {value!r}")
    return mapping[normalized]


def parse_int(value: object, default: int) -> int:
    if value in (None, ""):
        return default
    if isinstance(value, bool):
        return int(value)
    if isinstance(value, (int, float)):
        return int(value)
    return int(str(value).strip())


def parse_float(value: object, default: float) -> float:
    if value in (None, ""):
        return default
    if isinstance(value, (int, float)):
        return float(value)
    return float(str(value).strip())


def parse_optional_bool(value: object) -> bool | None:
    if value in (None, ""):
        return None
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return bool(value)
    token = str(value).strip().lower()
    if token in {"1", "true", "yes", "on"}:
        return True
    if token in {"0", "false", "no", "off"}:
        return False
    raise ValueError(f"Unknown boolean value: {value!r}")


def infer_material_from_type_hint(*hints: object) -> object:
    for hint in hints:
        token = str(hint or "").strip().lower()
        if "wood" in token:
            return "wood"
        if "stone" in token:
            return "stone"
        if "glass" in token:
            return "glass"
        if "ice" in token:
            return "ice"
    return None


def normalize_block_shape(value: object, has_polygon: bool) -> str:
    token = str(value or "").strip().lower()
    mapping = {
        "rect": "rect",
        "rectangle": "rect",
        "box": "rect",
        "circle": "circle",
        "ellipse": "circle",
        "triangle": "triangle",
        "tri": "triangle",
    }
    if token in mapping:
        return mapping[token]
    if has_polygon:
        return "triangle"
    return "rect"


def polygon_points(obj: dict) -> list[tuple[float, float]]:
    polygon = obj.get("polygon")
    if not isinstance(polygon, list) or len(polygon) < 3:
        return []

    points: list[tuple[float, float]] = []
    for point in polygon:
        if not isinstance(point, dict):
            continue
        px = point.get("x")
        py = point.get("y")
        if px is None or py is None:
            continue
        points.append((float(px), float(py)))

    if len(points) < 3:
        return []

    return points


def polygon_bounds(points: list[tuple[float, float]]) -> tuple[float, float, float, float] | None:
    if len(points) < 3:
        return None

    xs = [p[0] for p in points]
    ys = [p[1] for p in points]
    return min(xs), max(xs), min(ys), max(ys)


def triangle_twice_area(points: list[tuple[float, float]]) -> float:
    if len(points) != 3:
        return 0.0

    area2 = 0.0
    for idx in range(3):
        x1, y1 = points[idx]
        x2, y2 = points[(idx + 1) % 3]
        area2 += x1 * y2 - x2 * y1
    return area2


def normalize_projectile_type(value: object) -> str:
    normalized = str(value or "").strip().lower()
    mapping = {
        "striker": "Standard",
        "standard": "Standard",
        "splitter": "Splitter",
        "dasher": "Dasher",
        "bomber": "Bomber",
        "dropper": "Dropper",
        "boomerang": "Boomerang",
        "heavy": "Heavy",
        "bubbler": "Bubbler",
        "inflater": "Inflater",
    }
    if normalized not in mapping:
        raise ValueError(f"Unknown projectile type value: {value!r}")
    return mapping[normalized]


def parse_projectile_types(value: object) -> list[str]:
    if value in (None, ""):
        return []

    if isinstance(value, list):
        raw_values = value
    else:
        raw_values = re.split(r"[,;\n]+", str(value))

    projectile_types: list[str] = []
    for raw_value in raw_values:
        token = str(raw_value).strip()
        if not token:
            continue
        projectile_types.append(normalize_projectile_type(token))

    return projectile_types


def round_half_up(value: float) -> int:
    return int(math.floor(value + 0.5))


def is_potentially_destructible_block(block: dict[str, object]) -> bool:
    return not bool(block.get("static", False)) and not bool(
        block.get("indestructible", False)
    )


def compute_star_thresholds(
    targets: list[dict[str, object]], blocks: list[dict[str, object]]
) -> tuple[list[int], int, int, int]:
    target_score = sum(int(target.get("score", 0)) for target in targets)

    block_bonus_max = 0
    for block in blocks:
        if not is_potentially_destructible_block(block):
            continue
        material = str(block.get("material", "")).strip()
        bonus = MATERIAL_BLOCK_BONUS.get(material)
        if bonus is None:
            raise ValueError(f"Unknown block material for bonus scoring: {material!r}")
        block_bonus_max += bonus

    max_score = target_score + block_bonus_max

    star1 = target_score
    star2 = target_score + round_half_up(0.35 * block_bonus_max)
    star3 = target_score + round_half_up(0.70 * block_bonus_max)

    # Enforce strict monotonic thresholds while staying within max_score.
    if star2 <= star1:
        star2 = star1 + 1
    if star3 <= star2:
        star3 = star2 + 1
    if star3 > max_score:
        star3 = max_score
        star2 = min(star2, star3 - 1)

    if not (star1 < star2 < star3 <= max_score):
        raise ValueError(
            "Failed to derive strict star thresholds. Need enough potential block bonus so "
            "star1 < star2 < star3 <= maxScore is satisfiable."
        )

    return [star1, star2, star3], target_score, block_bonus_max, max_score


def infer_level_id(map_path: Path) -> int:
    match = re.search(r"(\d+)", map_path.stem)
    if match:
        return int(match.group(1))
    return 1


def to_center_position(obj: dict, width: float, height: float) -> list[int]:
    return to_center_position_with_local_center(obj, width / 2.0, height / 2.0)


def to_center_position_with_local_center(
    obj: dict, local_center_x: float, local_center_y: float
) -> list[int]:
    rotation_deg = float(obj.get("rotation", 0.0) or 0.0)
    rotation_rad = math.radians(rotation_deg)

    # Tiled rotates rectangle objects around their origin; for the object coordinates used in
    # this map, the rotated local center must follow the same sign convention as the stored
    # rectangle vertices.
    rotated_center_x = (
        local_center_x * math.cos(rotation_rad) - local_center_y * math.sin(rotation_rad)
    )
    rotated_center_y = (
        local_center_x * math.sin(rotation_rad) + local_center_y * math.cos(rotation_rad)
    )

    return [
        int(round(float(obj.get("x", 0.0)) + rotated_center_x)),
        int(round(float(obj.get("y", 0.0)) + rotated_center_y)),
    ]


def merge_properties(template_data: dict[str, object], obj: dict) -> dict[str, object]:
    merged: dict[str, object] = {}
    merged.update(template_data.get("properties", {}))
    merged.update(get_property_map(obj.get("properties")))
    return merged


def build_block(obj: dict, template_data: dict[str, object]) -> dict[str, object]:
    properties = merge_properties(template_data, obj)
    points = polygon_points(obj)
    has_polygon = len(points) >= 3
    shape = normalize_block_shape(properties.get("shape"), has_polygon)
    width = float(obj.get("width", template_data.get("width", 0.0)) or 0.0)
    height = float(obj.get("height", template_data.get("height", 0.0)) or 0.0)

    polygon_bbox = polygon_bounds(points)
    local_center_x = width / 2.0
    local_center_y = height / 2.0
    if polygon_bbox is not None:
        min_x, max_x, min_y, max_y = polygon_bbox
        if width <= 0.0:
            width = max_x - min_x
        if height <= 0.0:
            height = max_y - min_y
        local_center_x = (min_x + max_x) / 2.0
        local_center_y = (min_y + max_y) / 2.0
        position = to_center_position_with_local_center(
            obj, local_center_x, local_center_y
        )
    else:
        position = to_center_position(obj, width, height)

    material_hint = infer_material_from_type_hint(
        obj.get("type"),
        template_data.get("type"),
        Path(str(obj.get("template", ""))).stem,
    )
    material_source = properties.get("material", material_hint)

    block = {
        "shape": shape,
        "material": normalize_material(material_source),
        "position": position,
        "angle": int(round(float(obj.get("rotation", 0.0) or 0.0))),
        "hp": parse_int(properties.get("hp"), 50),
    }

    static_value = parse_optional_bool(
        properties.get("static", properties.get("isStatic"))
    )
    if static_value is not None:
        block["static"] = static_value

    indestructible_value = parse_optional_bool(
        properties.get("indestructible", properties.get("isIndestructible"))
    )
    if indestructible_value is not None:
        block["indestructible"] = indestructible_value

    if shape == "circle":
        radius_source = width if width > 0.0 else height
        block["radius"] = int(round(radius_source / 2.0))
    else:
        block["size"] = [int(round(width)), int(round(height))]

    if shape == "triangle" and polygon_bbox is not None:
        if len(points) != 3:
            raise ValueError(
                f"Triangle block object {obj.get('id')} must contain exactly 3 polygon points."
            )

        area2 = triangle_twice_area(points)
        if abs(area2) <= TRIANGLE_MIN_TWICE_AREA:
            raise ValueError(
                f"Triangle block object {obj.get('id')} is degenerate (zero area polygon)."
            )

        local_vertices: list[list[float]] = []
        for point_x, point_y in points:
            local_vertices.append(
                [
                    round(point_x - local_center_x, 3),
                    round(point_y - local_center_y, 3),
                ]
            )
        block["vertices"] = local_vertices

    return block


def build_target(obj: dict, template_data: dict[str, object]) -> dict[str, object]:
    properties = merge_properties(template_data, obj)
    width = float(obj.get("width", template_data.get("width", 0.0)) or 0.0)
    height = float(obj.get("height", template_data.get("height", 0.0)) or 0.0)
    radius = int(round(max(width, height) / 2.0))

    return {
        "position": to_center_position(obj, width, height),
        "radius": radius,
        "hp": parse_int(properties.get("hp"), 20),
        "score": parse_int(properties.get("score"), 1000),
    }


def build_slingshot(obj: dict, template_data: dict[str, object]) -> dict[str, object]:
    properties = merge_properties(template_data, obj)
    width = float(obj.get("width", template_data.get("width", 0.0)) or 0.0)
    height = float(obj.get("height", template_data.get("height", 0.0)) or 0.0)
    has_explicit_size = obj.get("width") is not None or obj.get("height") is not None

    if has_explicit_size and (width > 0.0 or height > 0.0):
        position = [
            int(round(float(obj.get("x", 0.0)) + width / 2.0)),
            int(round(float(obj.get("y", 0.0)) + height)),
        ]
    else:
        position = [
            int(round(float(obj.get("x", 0.0)))),
            int(round(float(obj.get("y", 0.0)))),
        ]

    return {
        "position": position,
        "maxPull": parse_int(properties.get("maxPull"), DEFAULT_MAX_PULL),
    }


def block_aabb(block: dict[str, object]) -> tuple[float, float, float, float]:
    cx = float(block["position"][0])
    cy = float(block["position"][1])
    if block["shape"] == "circle":
        r = float(block["radius"])
        return cx - r, cx + r, cy - r, cy + r
    if block["shape"] == "triangle" and isinstance(block.get("vertices"), list):
        xs: list[float] = []
        ys: list[float] = []
        for point in block["vertices"]:
            if not isinstance(point, list) or len(point) != 2:
                continue
            xs.append(cx + float(point[0]))
            ys.append(cy + float(point[1]))
        if len(xs) == 3 and len(ys) == 3:
            return min(xs), max(xs), min(ys), max(ys)
    w = float(block["size"][0])
    h = float(block["size"][1])
    return cx - w / 2.0, cx + w / 2.0, cy - h / 2.0, cy + h / 2.0


def aabb_intersects(
    lhs: tuple[float, float, float, float], rhs: tuple[float, float, float, float]
) -> bool:
    # Treat edge contact as non-overlap: slingshot base may rest on a block top.
    return not (
        lhs[1] <= rhs[0] or rhs[1] <= lhs[0] or lhs[3] <= rhs[2] or rhs[3] <= lhs[2]
    )


def warn_if_slingshot_occluded(
    slingshot: dict[str, object], blocks: list[dict[str, object]], warnings: list[str]
) -> None:
    # Matches renderer dimensions approximately: trunk width 14 px, fork spread +/-10 px,
    # total visible height ~62 px above base.
    sx = float(slingshot["position"][0])
    sy = float(slingshot["position"][1])
    # Ignore exact contact at the base y: this lets the slingshot stand on a block top.
    slingshot_box = (sx - 10.0, sx + 10.0, sy - 62.0, sy - 1.0)

    for index, block in enumerate(blocks):
        if aabb_intersects(slingshot_box, block_aabb(block)):
            warnings.append(
                "Slingshot overlaps block "
                f"#{index} after conversion (slingshot={slingshot['position']}). "
                "Move slingshot left/up in Tiled or move nearby blocks."
            )
            break


def collect_layer_objects(layers: list[dict], layer_name: str) -> list[dict]:
    for layer in layers:
        if layer.get("name") == layer_name:
            return list(layer.get("objects", []))
    return []


def convert_map(map_path: Path) -> tuple[dict[str, object], list[str]]:
    tiled_map = load_json(map_path)
    warnings: list[str] = []
    map_properties = get_property_map(tiled_map.get("properties"))

    inferred_level_id = infer_level_id(map_path)
    level_id = parse_int(map_properties.get("level_id"), 0)
    if level_id <= 0:
        level_id = inferred_level_id
        warnings.append(f"Map property 'level_id' missing/invalid, using inferred id {level_id}.")

    level_name = str(map_properties.get("level_name") or "").strip()
    if not level_name:
        level_name = f"Level {level_id}"
        warnings.append(f"Map property 'level_name' missing/empty, using '{level_name}'.")

    if any(
        key in map_properties
        for key in ("star_1", "star_2", "star_3", "starThresholds")
    ):
        warnings.append(
            "Map star threshold properties are ignored. Thresholds are auto-generated "
            "from targets and destructible blocks."
        )

    projectile_types = parse_projectile_types(map_properties.get("projectile_types"))

    total_shots = parse_int(map_properties.get("total_shots"), 0)
    if projectile_types:
        if total_shots > 0 and total_shots != len(projectile_types):
            warnings.append(
                "Map property 'total_shots' does not match projectile_types length, "
                f"using projectile_types length {len(projectile_types)}."
            )
        total_shots = len(projectile_types)
    elif total_shots <= 0:
        total_shots = DEFAULT_TOTAL_SHOTS
        warnings.append(
            f"Map property 'total_shots' missing/invalid, using default {total_shots}."
        )

    blocks: list[dict[str, object]] = []
    for obj in collect_layer_objects(tiled_map.get("layers", []), "blocks"):
        template_data = {}
        template_ref = obj.get("template")
        if isinstance(template_ref, str) and template_ref:
            template_path = resolve_template_path(map_path, template_ref)
            if template_path.exists():
                template_data = load_template_data(template_path)
            else:
                warnings.append(f"Template not found for block object {obj.get('id')}: {template_ref}")
        blocks.append(build_block(obj, template_data))

    targets: list[dict[str, object]] = []
    for obj in collect_layer_objects(tiled_map.get("layers", []), "targets"):
        template_data = {}
        template_ref = obj.get("template")
        if isinstance(template_ref, str) and template_ref:
            template_path = resolve_template_path(map_path, template_ref)
            if template_path.exists():
                template_data = load_template_data(template_path)
            else:
                warnings.append(
                    f"Template not found for target object {obj.get('id')}: {template_ref}"
                )
        targets.append(build_target(obj, template_data))

    if not targets:
        warnings.append("Layer 'targets' has no objects; generated level will auto-win.")

    star_thresholds, target_score, block_bonus_max, max_score = compute_star_thresholds(
        targets, blocks
    )
    warnings.append(
        "Auto star thresholds: "
        f"targetScore={target_score}, blockBonusMax={block_bonus_max}, "
        f"maxScore={max_score}, stars={star_thresholds}."
    )

    slingshot_objects = collect_layer_objects(tiled_map.get("layers", []), "slingshot")
    if not slingshot_objects:
        raise ValueError("Layer 'slingshot' must contain exactly one object.")
    if len(slingshot_objects) > 1:
        warnings.append("Layer 'slingshot' has multiple objects; using the first one.")
    slingshot_template_data = {}
    slingshot_template_ref = slingshot_objects[0].get("template")
    if isinstance(slingshot_template_ref, str) and slingshot_template_ref:
        slingshot_template_path = resolve_template_path(map_path, slingshot_template_ref)
        if slingshot_template_path.exists():
            slingshot_template_data = load_template_data(slingshot_template_path)
        else:
            warnings.append(
                f"Template not found for slingshot object {slingshot_objects[0].get('id')}: "
                f"{slingshot_template_ref}"
            )

    slingshot = build_slingshot(slingshot_objects[0], slingshot_template_data)
    raw_slingshot_width = parse_float(slingshot_objects[0].get("width"), 0.0)
    raw_slingshot_height = parse_float(slingshot_objects[0].get("height"), 0.0)
    if raw_slingshot_width > 0.0 or raw_slingshot_height > 0.0:
        warnings.append(
            "Slingshot object has explicit width/height. Converter treats x/y as top-left "
            "and uses bottom-center anchor. Prefer point-like slingshot objects "
            "(width=0, height=0) for predictable placement."
        )
    if slingshot["maxPull"] == DEFAULT_MAX_PULL and "maxPull" not in merge_properties(
        slingshot_template_data, slingshot_objects[0]
    ):
        warnings.append(
            f"Slingshot property 'maxPull' missing, using default {DEFAULT_MAX_PULL}."
        )
    warn_if_slingshot_occluded(slingshot, blocks, warnings)

    level = {
        "meta": {
            "id": level_id,
            "name": level_name,
            "totalShots": total_shots,
            "starThresholds": star_thresholds,
        },
        "slingshot": slingshot,
        "projectiles": (
            [{"type": projectile_type} for projectile_type in projectile_types]
            if projectile_types
            else [{"type": "Standard"} for _ in range(total_shots)]
        ),
        "blocks": blocks,
        "targets": targets,
    }

    return level, warnings


def default_output_path(input_path: Path, level_id: int) -> Path:
    return REPO_ROOT / "levels" / f"level_{level_id:02d}.json"


def main() -> int:
    args = parse_args()
    level, warnings = convert_map(args.input.resolve())
    output_path = args.output or default_output_path(args.input.resolve(), level["meta"]["id"])
    output_path = output_path.resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    with output_path.open("w", encoding="utf-8") as handle:
        json.dump(level, handle, indent=2)
        handle.write("\n")

    print(f"Wrote {output_path}")
    for warning in warnings:
        print(f"WARNING: {warning}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
