import json
import zipfile
from pathlib import Path
from xml.etree import ElementTree


ROOT = Path(__file__).resolve().parents[2]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"Stage 14B-03A fixture guard: FAIL: {message}")


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def validate_checker_3mf() -> None:
    package = ROOT / "samples/models/3mf/texture2d_checker_cube.3mf"
    with zipfile.ZipFile(package) as archive:
        names = set(archive.namelist())
        require("3D/Textures/checker.png" in names,
                "checker 3MF texture part is missing")
        model_parts = [name for name in names
                       if name.lower().endswith(".model")]
        require(len(model_parts) == 1,
                "checker 3MF must expose one model part")
        root = ElementTree.fromstring(archive.read(model_parts[0]))

    textures = [node for node in root.iter()
                if local_name(node.tag) == "texture2d"]
    groups = [node for node in root.iter()
              if local_name(node.tag) == "texture2dgroup"]
    triangles = [node for node in root.iter()
                 if local_name(node.tag) == "triangle"]
    require(len(textures) == 1 and len(groups) == 1,
            "checker 3MF texture resource closure changed")
    group_id = groups[0].attrib.get("id")
    require(all(triangle.attrib.get("pid") == group_id
                for triangle in triangles),
            "checker 3MF triangles must bind the Texture2DGroup")
    require(all(key in triangle.attrib
                for triangle in triangles
                for key in ("p1", "p2", "p3")),
            "checker 3MF triangle UV references are incomplete")


def validate_shengdanjie() -> None:
    directory = ROOT / "model/obj/shengdanjie_fudiao/star"
    obj = directory / "MF_shengdanjie_zhongzhi_R_fy02.obj"
    mtl = directory / "MF_shengdanjie_zhongzhi_R_fy02.mtl"
    used = {
        line.split(maxsplit=1)[1].strip()
        for line in obj.read_text(encoding="utf-8", errors="ignore").splitlines()
        if line.startswith("usemtl ")
    }
    material_textures: dict[str, str] = {}
    current = ""
    for line in mtl.read_text(encoding="utf-8", errors="ignore").splitlines():
        if line.startswith("newmtl "):
            current = line.split(maxsplit=1)[1].strip()
        elif line.startswith("map_Kd ") and current:
            material_textures[current] = line.split(maxsplit=1)[1].strip()

    require(used == {"MI_shengdanjie_zhongzhi_R_fy2"},
            "shengdanjie used-material fixture changed")
    require(all(name in material_textures for name in used),
            "shengdanjie used material has no texture binding")
    require(all((directory / material_textures[name]).is_file()
                for name in used),
            "shengdanjie used texture is missing")
    unused_missing = [texture for name, texture in material_textures.items()
                      if name not in used and not (directory / texture).is_file()]
    require(unused_missing,
            "shengdanjie fixture no longer exercises unused missing textures")


def validate_dual_model_scene() -> None:
    scene_path = ROOT / "samples/configs/scene/fixture_two_model_scene.json"
    scene = json.loads(scene_path.read_text(encoding="utf-8"))
    models = scene.get("models", [])
    instances = scene.get("instances", [])
    require(len(models) == 2 and len(instances) == 2,
            "dual-model scene must retain two models and instances")
    model_ids = {model["modelId"] for model in models}
    require(len(model_ids) == 2,
            "dual-model scene model identities must remain distinct")
    require(all(instance["modelId"] in model_ids for instance in instances),
            "dual-model instances must resolve to declared models")
    require(all((ROOT / model["sourcePath"]).is_file() for model in models),
            "dual-model source asset is missing")


def main() -> None:
    validate_checker_3mf()
    validate_shengdanjie()
    validate_dual_model_scene()
    print("Stage 14B-03A real fixture guard: PASS")


if __name__ == "__main__":
    main()
