""" external/extensions/external_archive.bzl """

load("@fly//scripts/bazel/rules:external_archive.bzl", "get_external_archive")

def _external_archive_impl(module_ctx):
    for module in module_ctx.modules:
        for attr in module.tags.base:
            attrs = {
                key: getattr(attr, key)
                for key in dir(attr)
                if not key.startswith("_")
            }
            get_external_archive(
                name = attrs["name"],
                url = attrs["url"],
                sha256 = attrs["sha256"],
                build_file = attrs["build_file"],
            )

_external_archive_attrs = {
    "name": attr.string(mandatory = True),
    "url": attr.string(mandatory = True),
    "sha256": attr.string(mandatory = True),
    "build_file": attr.string(mandatory = False, default = ""),
}

external_archive = module_extension(
    implementation = _external_archive_impl,
    tag_classes = {
        "base": tag_class(
            attrs = _external_archive_attrs,
         ),
    },
)

