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

def _cook_meshes_impl(ctx):
    outs = []
    for f in ctx.files.outputs:
        out = ctx.actions.declare_file(f.basename)
        outs.append(out)

    extra_options = []
    extra_options.append("-s")
    extra_options.append(ctx.attr.scale)
    extra_options.append("-c")
    extra_options.append(ctx.attr.coord_system)
    if ctx.attr.flip_forward:
        extra_options.append("-ff")

    ctx.actions.run(
        inputs = ctx.files.inputs,
        outputs = outs,
        executable = ctx.executable._command,
        arguments = [
            "-i"
        ] +
        [f.path for f in ctx.files.inputs] +
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

cook_meshes = rule(
    implementation = _cook_meshes_impl,
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
            default = Label("//src/assets/geometry:cooker"),
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
    },
)

