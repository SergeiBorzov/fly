load ("@bazel_skylib//lib:paths.bzl", "paths")

def _copy_files_to_directory_impl(ctx):
    outputs = []
    for file in ctx.files.srcs:
        outputs.append(ctx.actions.declare_file(paths.join(ctx.attr.output_directory, file.basename)))

    ctx.actions.run(
        inputs = ctx.files.srcs,
        outputs = outputs,
        executable = ctx.executable._command,
        arguments = ["-i"] + [file.path for file in ctx.files.srcs] + ["-o"] + [file.path for file in outputs]
    )

    return DefaultInfo(
        files = depset(outputs),
        runfiles = ctx.runfiles(outputs),
    )

copy_files_to_directory = rule(
    implementation = _copy_files_to_directory_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = True,
        ),
        "output_directory": attr.string(),
        "_command": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("@fly//scripts/python:copy_files"),
        ),
    },
)
