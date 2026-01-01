def _cook_images_impl(ctx):
    outs = []
    for f in ctx.files.outputs:
        out = ctx.actions.declare_file(f.basename)
        outs.append(out)

    extra_options = []
    if ctx.attr.generate_mips:
        extra_options.append("-m")
    if ctx.attr.eq2cube:
        extra_options.append("-eq2cube")

    ctx.actions.run(
        inputs = ctx.files.inputs,
        outputs = outs,
        executable = ctx.executable._command,
        arguments = [
            "-i"
        ] +
        [f.path for f in ctx.files.inputs] +
        extra_options + ["-o"] + [f.path for f in outs],
        #env = {
        #    "VK_LOADER_DEBUG": "all",
        #},
    )

    return [DefaultInfo(files = depset(outs))]

def _cook_scenes_impl(ctx):
    outs = []
    for f in ctx.files.outputs:
        out = ctx.actions.declare_file(f.basename)
        outs.append(out)

    inputs = []
    for f in ctx.files.inputs:
        if f.path.endswith(".obj") or f.path.endswith(".OBJ") or \
           f.path.endswith(".gltf") or f.path.endswith(".GLTF") or \
           f.path.endswith(".glb") or f.path.endswith(".GLB"):
            inputs.append(f.path)
            
    extra_options = []
    extra_options.append("-s")
    extra_options.append(ctx.attr.scale)
    extra_options.append("-c")
    extra_options.append(ctx.attr.coord_system)
    if ctx.attr.flip_forward:
        extra_options.append("-ff")
    if ctx.attr.flip_winding_order:
        extra_options.append("-fw")
    if not ctx.attr.export_nodes:
        extra_options.append("-nn")
    if not ctx.attr.export_materials:
        extra_options.append("-nm")

    ctx.actions.run(
        inputs = ctx.files.inputs,
        outputs = outs,
        executable = ctx.executable._command,
        arguments = [
            "-i"
        ] +
        [f for f in inputs] +
        extra_options + ["-o"] + [f.path for f in outs],
    )

    return [DefaultInfo(files = depset(outs))]

def _file_to_cpp_header_impl(ctx):
    outs = [ctx.actions.declare_file(ctx.label.name)]

    extra_options = []
    extra_options.append("-n")
    extra_options.append(ctx.attr.array_name)
    extra_options.append("-a")
    extra_options.append(str(ctx.attr.alignment))
    
    ctx.actions.run(
        inputs = [ctx.file.input],
        outputs = outs,
        executable = ctx.executable._command,
        arguments = [
                        "-i",
                    ] +
                    [ctx.file.input.path] +
                    extra_options + ["-o"] + [f.path for f in outs],
    )

    return [DefaultInfo(files = depset(outs))]

cook_images = rule(
    implementation = _cook_images_impl,
    attrs = {
        "inputs": attr.label_list(
            mandatory = True,
            allow_empty = False,
            allow_files = True,
        ),
        "outputs": attr.label_list(
            mandatory = True,
            allow_empty = False,
            allow_files = True,
        ),
        "generate_mips": attr.bool(),
        "eq2cube": attr.bool(),
        "_command": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("//src/assets/image:cooker"),
        ),
    },
)

cook_scenes = rule(
    implementation = _cook_scenes_impl,
    attrs = {
        "inputs": attr.label_list(
            mandatory = True,
            allow_empty = False,
            allow_files = True,
        ),
        "outputs": attr.label_list(
            mandatory = True,
            allow_empty = False,
            allow_files = True,
        ),
        "_command": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("//src/assets/scene:cooker"),
        ),
        "scale": attr.string(
            default = "1.0"
        ),
        "coord_system": attr.string(
            default = "xyz",
            values = ["xyz", "xzy", "yxz", "yzx", "zxy", "zyx"],
        ),
        "flip_forward": attr.bool(
            default = False,
        ),
        "flip_winding_order": attr.bool(
            default = False,
        ),
        "export_nodes": attr.bool(
            default = True,
        ),
        "export_materials": attr.bool(
            default = True,
        ),
    },
)

file_to_cpp_header = rule(
    implementation = _file_to_cpp_header_impl,
    attrs = {
        "input": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
        "alignment": attr.int(
            default = 0,
            mandatory = False,
        ),
        "array_name": attr.string(
            mandatory = True,
        ),
        "_command": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("//src/assets/tools:hex_dump"),
        ),
    },
)
