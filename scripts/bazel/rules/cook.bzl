def _cook_image_impl(ctx):
    outs = []
    for f in ctx.files.inputs:
        out_name = f.basename.rsplit(".", 1)[0] + "." + ctx.attr.codec
        out = ctx.actions.declare_file(out_name)
        outs.append(out)
        
    ctx.actions.run(
        inputs = ctx.files.inputs,
        outputs = outs,
        executable = ctx.executable._command,
        arguments = [
            "-c",
            ctx.attr.codec,
            "-i"
        ] +
        [f.path for f in ctx.files.inputs] + [
            "-o"
        ]
        + [f.path for f in outs],
    )

    return [DefaultInfo(files = depset(outs))]

cook_image = rule(
    implementation = _cook_image_impl,
    attrs = {
        "codec": attr.string(
            mandatory = True,
            values = ["bc1", "bc3", "bc4", "bc5"],
        ),
        "inputs": attr.label_list(
            mandatory = True,
            allow_empty = False,
            allow_files = True,
        ),
        "_command": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("//src/assets:cooker"),
        ),
    },
)