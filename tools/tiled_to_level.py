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
            value = prop.get("value")
            if value is None and prop.text is not None:
                value = prop.text
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


def normalize_projectile_type(value: object) -> str:
    normalized = str(value or "").strip().lower()
    mapping = {
        "striker": "Striker",
        "standard": "Striker",
        "splitter": "Splitter",
        "dasher": "Dasher",
        "bomber": "Bomber",
        "dropper": "Dropper",
        "boomerang": "Boomerang",
        "crusher": "Crusher",
        "heavy": "Crusher",
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


def infer_level_id(map_path: Path) -> int:
    match = re.search(r"(\d+)", map_path.stem)
    if match:
        return int(match.group(1))
    return 1


def to_center_position(obj: dict, width: float, height: float) -> list[int]:
    rotation_deg = float(obj.get("rotation", 0.0) or 0.0)
    rotation_rad = math.radians(rotation_deg)
    local_center_x = width / 2.0
    local_center_y = height / 2.0

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
    width = float(obj.get("width", template_data.get("width", 0.0)) or 0.0)
    height = float(obj.get("height", template_data.get("height", 0.0)) or 0.0)
    shape = str(properties.get("shape", "rect")).strip().lower()

    block = {
        "shape": shape,
        "material": normalize_material(properties.get("material")),
        "position": to_center_position(obj, width, height),
        "angle": int(round(float(obj.get("rotation", 0.0) or 0.0))),
        "hp": parse_int(properties.get("hp"), 50),
    }

    if shape == "circle":
        block["radius"] = int(round(width / 2.0))
    else:
        block["shape"] = "rect"
        block["size"] = [int(round(width)), int(round(height))]

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

    if width > 0.0 or height > 0.0:
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

    star_thresholds = [
        parse_int(map_properties.get("star_1"), 0),
        parse_int(map_properties.get("star_2"), 0),
        parse_int(map_properties.get("star_3"), 0),
    ]
    if any(value <= 0 for value in star_thresholds) or not (
        star_thresholds[0] < star_thresholds[1] < star_thresholds[2]
    ):
        star_thresholds = DEFAULT_STAR_THRESHOLDS[:]
        warnings.append(
            "Map star thresholds missing/invalid, using default thresholds "
            f"{star_thresholds}."
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
    if slingshot["maxPull"] == DEFAULT_MAX_PULL and "maxPull" not in merge_properties(
        slingshot_template_data, slingshot_objects[0]
    ):
        warnings.append(
            f"Slingshot property 'maxPull' missing, using default {DEFAULT_MAX_PULL}."
        )

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
            else [{"type": "Striker"} for _ in range(total_shots)]
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
