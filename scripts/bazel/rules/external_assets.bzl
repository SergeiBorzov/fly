""" external/rules/external_assets.bzl """

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")

def get_filename_from_url(url):
    if "?" in url:
        parts = url.split("?")
        query = parts[1]
        for param in query.split("&"):
            if param.startswith("file="):
                return param[len("file="):]
    path = url.split("/")[-1]
    return path

def get_external_assets_impl(ctx):
    urls = ctx.attr.urls
    hashes = ctx.attr.sha256
    build_file = ctx.attr.build_file
    names = [get_filename_from_url(url) for url in urls]

    for i in range(0, len(urls)):
        if urls[i].endswith(".zip"):
            archive_path = ctx.path(names[i])
            extract_dir = ctx.path(names[i][:-4]) # remove .zip

            ctx.download(
                url = urls[i],
                output = archive_path,
                sha256 = hashes[i],
            )
            ctx.extract(
                archive = archive_path,
                output = extract_dir
            )
        else:
            out_file = ctx.path(names[i])
            ctx.download(
                url = urls[i],
                output = out_file,
                sha256 = hashes[i],
            )

    ctx.file("BUILD.bazel", ctx.read(ctx.attr.build_file))

get_external_assets = repository_rule(
    implementation = get_external_assets_impl,
    attrs = {
        "urls": attr.string_list(),
        "sha256": attr.string_list(),
        "build_file": attr.label(),
    },
)