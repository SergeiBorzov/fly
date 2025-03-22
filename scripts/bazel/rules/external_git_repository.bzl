""" external/rules/external_git_repository.bzl """

load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")

def get_external_git_repository(name, commit, remote, build_file):
    if build_file != "":
        new_git_repository(
            name = name,
            commit = commit,
            remote = remote,
            build_file = build_file,
        )
    else:
        new_git_repository(
            name = name,
            commit = commit,
            remote = remote,
        )

