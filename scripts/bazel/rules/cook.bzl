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
            default = Label("//src/assets/cooker:cooker"),
        ),
    },
)
