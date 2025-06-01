""" external/extensions/external_git_repository.bzl """

load("@fly//scripts/bazel/rules:external_git_repository.bzl", "get_external_git_repository")

def _external_git_repository_impl(module_ctx):
    for module in module_ctx.modules:
        for attr in module.tags.base:
            attrs = {
                key: getattr(attr, key)
                for key in dir(attr)
                if not key.startswith("_")
            }
            get_external_git_repository(
                name = attrs["name"],
                commit = attrs["commit"],
                remote = attrs["remote"],
                build_file = attrs["build_file"],
            )

_external_git_repository_attrs = {
    "name": attr.string(mandatory = True),
    "commit": attr.string(mandatory = True),
    "remote": attr.string(mandatory = True),
    "build_file": attr.string(mandatory = False, default = ""),
}

external_git_repository = module_extension(
    implementation = _external_git_repository_impl,
    tag_classes = {
        "base": tag_class(
            attrs = _external_git_repository_attrs,
         ),
    },
)

