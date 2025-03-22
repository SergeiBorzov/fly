""" external/extensions/external_data.bzl """

load("@helios//scripts/bazel/rules:external_data.bzl", "get_external_data")

def _external_data_impl(module_ctx):
    for module in module_ctx.modules:
        for attr in module.tags.base:
            attrs = {
                key: getattr(attr, key)
                for key in dir(attr)
                if not key.startswith("_")
            }
            get_external_data(
                name = attrs["name"],
                url = attrs["url"],
                sha256 = attrs["sha256"],
                build_file = attrs["build_file"],
            )

_external_data_attrs = {
    "name": attr.string(mandatory = True),
    "url": attr.string(mandatory = True),
    "sha256": attr.string(mandatory = True),
    "build_file": attr.string(mandatory = False, default = ""),
}

external_data = module_extension(
    implementation = _external_data_impl,
    tag_classes = {
        "base": tag_class(
            attrs = _external_data_attrs,
         ),
    },
)

