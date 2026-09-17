
import argparse
import fileinput
import os
import subprocess
import sys
import time
from shutil import move
# To match required file names to fother shading languages that don't support multiple entry points, shader files may need to be renamed for some samples
def checkRenameFiles(samplename):
    mappings = {}
    match samplename:
        case "displacement":
            mappings = {
                "displacement.vert.spv": "base.vert.spv",
                "displacement.frag.spv": "base.frag.spv",
            }        
        case "geometryshader":
            mappings = {
                "normaldebug.vert.spv": "base.vert.spv",
                "normaldebug.frag.spv": "base.frag.spv",
            }
        case "graphicspipelinelibrary":
            mappings = {
                "uber.vert.spv": "shared.vert.spv",
            }             
        case "raytracingbasic":
            mappings = {
                "raytracingbasic.rchit.spv": "closesthit.rchit.spv",
                "raytracingbasic.rmiss.spv": "miss.rmiss.spv",
                "raytracingbasic.rgen.spv": "raygen.rgen.spv",
            }
        case "raytracingcallable":
            mappings = {
                "raytracingcallable.rchit.spv": "closesthit.rchit.spv",
                "raytracingcallable.rmiss.spv": "miss.rmiss.spv",
                "raytracingcallable.rgen.spv": "raygen.rgen.spv",
            }            
        case "raytracinggltf":
            mappings = {
                "raytracinggltf.rchit.spv": "closesthit.rchit.spv",
                "raytracinggltf.rmiss.spv": "miss.rmiss.spv",
                "raytracinggltf.rgen.spv": "raygen.rgen.spv",
                "raytracinggltf.rahit.spv": "anyhit.rahit.spv",
            }
        case "raytracingpositionfetch":
            mappings = {
                "raytracingpositionfetch.rchit.spv": "closesthit.rchit.spv",
                "raytracingpositionfetch.rmiss.spv": "miss.rmiss.spv",
                "raytracingpositionfetch.rgen.spv": "raygen.rgen.spv",
            }                     
        case "raytracingreflections":
            mappings = {
                "raytracingreflections.rchit.spv": "closesthit.rchit.spv",
                "raytracingreflections.rmiss.spv": "miss.rmiss.spv",
                "raytracingreflections.rgen.spv": "raygen.rgen.spv",
            }
        case "raytracingsbtdata":
            mappings = {
                "raytracingsbtdata.rchit.spv": "closesthit.rchit.spv",
                "raytracingsbtdata.rmiss.spv": "miss.rmiss.spv",
                "raytracingsbtdata.rgen.spv": "raygen.rgen.spv",
            }             
        case "raytracingshadows":
            mappings = {
                "raytracingshadows.rchit.spv": "closesthit.rchit.spv",
                "raytracingshadows.rmiss.spv": "miss.rmiss.spv",
                "raytracingshadows.rgen.spv": "raygen.rgen.spv",
            }
        case "raytracingtextures":
            mappings = {
                "raytracingtextures.rchit.spv": "closesthit.rchit.spv",
                "raytracingtextures.rmiss.spv": "miss.rmiss.spv",
                "raytracingtextures.rgen.spv": "raygen.rgen.spv",
                "raytracingtextures.rahit.spv": "anyhit.rahit.spv",
            }            
        case "raytracingintersection":
            mappings = {
                "raytracingintersection.rchit.spv": "closesthit.rchit.spv",
                "raytracingintersection.rmiss.spv": "miss.rmiss.spv",
                "raytracingintersection.rgen.spv": "raygen.rgen.spv",
                "raytracingintersection.rint.spv": "intersection.rint.spv",
            }                   
        case "viewportarray":
            mappings = {
                "scene.geom.spv": "multiview.geom.spv",
            }
    for x, y in mappings.items():
        move(os.path.join(samplename, x), os.path.join(samplename, y))

parser = argparse.ArgumentParser(description='Compile all slang shaders')
parser.add_argument('--slangc', type=str, help='path to slangc executable')
parser.add_argument('--outputDir', type=str, help='compile shaders output path')
parser.add_argument('--sample', type=str, help='compile shaders for a single sample only')
parser.add_argument('--force', action='store_true', help='force recompile all shaders')
args = parser.parse_args()

compiler_path = "..\\thirdParty\\slang\\bin\\slangc.exe"
if args.slangc:
    compiler_path = args.slangc
print(f"slang编译器路径: {compiler_path}")

# 检查输出目录是否存在
out_dir = "../build/bin/shaders/"
if args.outputDir:
    out_dir = args.outputDir + "/"
if not os.path.exists(out_dir):
    os.makedirs(out_dir, exist_ok=True)
    print(f"创建输出目录: {os.path.abspath(out_dir)}")
print(f"\nshader编译输出目录: {os.path.abspath(out_dir)}")

# 是否强制重新生成所有shader，否则仅生成发生变动的shader（通过时间戳判断是否发生了变动）
force_compile = False
if args.force:
    force_compile = True
    print(f"\n重新生成所有shader")
else:
    print(f"\n更新所有shader")

def getShaderStages(filename):
    stages = []
    stage_tags = {
        'vertex': '[shader("vertex")]',
        'fragment': '[shader("fragment")]',
        'raygeneration': '[shader("raygeneration")]',
        'miss': '[shader("miss")]',
        'closesthit': '[shader("closesthit")]',
        'callable': '[shader("callable")]',
        'intersection': '[shader("intersection")]',
        'anyhit': '[shader("anyhit")]',
        'compute': '[shader("compute")]',
        'amplification': '[shader("amplification")]',
        'mesh': '[shader("mesh")]',
        'geometry': '[shader("geometry")]',
        'hull': '[shader("hull")]',
        'domain': '[shader("domain")]'
    }
    
    try:
        with open(filename, 'r', encoding='utf-8') as f:
            content = f.read()
            for stage, tag in stage_tags.items():
                if tag in content:
                    stages.append(stage)
    except Exception as e:
        print(f"Error reading {filename}: {str(e)}")
    
    return stages

# 计时开始
total_start = time.time()
compiled_count = 0
skipped_count = 0

compile_target = args.sample if args.sample else ""
if compile_target and not os.path.isdir(compile_target):
    print(f"ERROR: Sample directory not found: {compile_target}")
    sys.exit(1)

base_dir = os.path.dirname(os.path.realpath(__file__))
for root, dirs, files in os.walk(base_dir):
    sample_name = os.path.basename(root)
    if compile_target and sample_name != compile_target:
        continue
        
    shader_files = [f for f in files if f.endswith(".slang")]
    if not shader_files:
        continue
        
    print("\n开始编译：")
    for file in shader_files:
        input_path = os.path.join(root, file)
        stages = getShaderStages(input_path)
        if not stages:
            continue
            
        needs_compile = False
        output_files = []
        
        # 检查每个阶段是否需要编译
        for stage in stages:
            ext_map = {
                "vertex": ".vert",
                "fragment": ".frag",
                "raygeneration": ".rgen",
                "miss": ".rmiss",
                "closesthit": ".rchit",
                "callable": ".rcall",
                "intersection": ".rint",
                "anyhit": ".rahit",
                "compute": ".comp",
                "mesh": ".mesh",
                "amplification": ".task",
                "geometry": ".geom",
                "hull": ".tesc",
                "domain": ".tese"
            }
            output_ext = ext_map.get(stage, "")
            if not output_ext:
                continue
                
            output_path = out_dir + file + output_ext + ".spv"
            output_path = output_path.replace(".slang", "")
            output_files.append(output_path)
            
            # 检查是否需要编译
            if force_compile or not os.path.exists(output_path):
                needs_compile = True
            else:
                # 比较时间戳
                src_time = os.path.getmtime(input_path)
                dst_time = os.path.getmtime(output_path)
                if src_time > dst_time:
                    needs_compile = True
        
        if not needs_compile:
            print(f" {file} (Skip)")
            skipped_count += len(stages)
            continue
            
        # 编译所有阶段
        print(f" {file}")
        for stage in stages:
            output_ext = ext_map.get(stage, "")
            if not output_ext:
                continue

            output_path = out_dir + file + output_ext + ".spv"
            output_path = output_path.replace(".slang", "")
            entry_point = stage + "Main"
            cmd = [
                compiler_path,
                input_path,
                "-profile", "spirv_1_4",
                "-matrix-layout-column-major",
                "-target", "spirv",
                "-o", output_path,
                "-entry", entry_point,
                "-stage", stage,
                "-warnings-disable", "39001"
            ]
            
            try:
                result = subprocess.run(cmd, capture_output=True, text=True)
                if result.returncode == 0:
                    print(f"     {stage} -> {output_path}")
                    compiled_count += 1
                else:
                    print(f"    [x] {stage} failed!")
                    print(f"        Command: {' '.join(cmd)}")
                    print(f"        Error: {result.stderr.strip()}")
                    sys.exit(result.returncode)
            except Exception as e:
                print(f"    [x] {stage} exception: {str(e)}")
                sys.exit(1)    
    checkRenameFiles(sample_name)
    print("结束编译")

# 输出统计信息
total_time = time.time() - total_start
print("\n编译情况统计:")
print(f"  Total shader stages: {compiled_count + skipped_count}")
print(f"  Compiled: {compiled_count}")
print(f"  Skipped: {skipped_count}")
print(f"  Time: {total_time:.2f} seconds")
