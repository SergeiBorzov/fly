""" external/rules/external_git_repository.bzl """

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def get_external_data(name, url, sha256, build_file):
    if build_file != "":
        http_archive(
            name = name,
            url = url,
            sha256 = sha256,
            build_file = build_file
        )
    else:
        http_archive(
            name = name,
            url = url,
            sha256 = sha256,
        )
