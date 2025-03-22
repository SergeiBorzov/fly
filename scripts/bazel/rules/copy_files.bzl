""" external/rules/copy_files.bzl """
load ("@bazel_skylib//lib:paths.bzl", "paths")

def _copy_files_impl(ctx):
    outputs = []
    for file in ctx.files.srcs:
        out_file = ctx.actions.declare_file(paths.join(ctx.attr.output_directory, file.basename))
        outputs.append(out_file)

        #args = ctx.actions.args()
        #args.add(file)
        #args.add(out_file)

        # if ctx.configuration.host_path_separator == "\\":
        #     command = "copy {} {}".format(file.path, out_file.path)
        # else:
        command = "cp {} {}".format(file.path, out_file.path)

        ctx.actions.run_shell(
            inputs = [file],
            outputs = [out_file],
            command = command,
            #arguments = [args],
        )

    return DefaultInfo(
        files = depset(outputs),
        runfiles = ctx.runfiles(outputs),
    )

copy_files = rule(
    implementation = _copy_files_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = True,
        ),
        "output_directory": attr.string(),
        "_command": attr.label(
            cfg = "exec",
            executable = True,
            default = Label("")
    },
)
