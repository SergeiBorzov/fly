""" external/extensions/external_assets.bzl """

load("@fly//scripts/bazel/rules:external_assets.bzl", "get_external_assets")

def _external_assets_impl(module_ctx):
    for module in module_ctx.modules:
        for attr in module.tags.base:
            attrs = {
                key: getattr(attr, key)
                for key in dir(attr)
                if not key.startswith("_")
            }

            get_external_assets(
                name = attrs["name"],
                urls = attrs["urls"],
                sha256 = attrs["sha256"],
                build_file = attrs["build_file"]
            )

_external_assets_attrs = {
    "name": attr.string(mandatory = True),
    "urls": attr.string_list(mandatory = True),
    "sha256": attr.string_list(mandatory = True),
    "build_file": attr.label(mandatory = True, allow_single_file=True),
}

external_assets = module_extension(
    implementation = _external_assets_impl,
    tag_classes = {
        "base": tag_class(
            attrs = _external_assets_attrs,
         ),
    },
)