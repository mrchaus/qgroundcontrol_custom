#!/usr/bin/bash
#
# Premise:
# Shared libraries (Qt, airmap, etc) on Linux are built without knowing where
# they will be installed to; they assume they will be installed to the system.
#
# QGC does not install to the system. Instead it copies them to `Qt/libs`. The
# libraries need to have their rpath set correctly to ensure that the built
# application and its libraries are found at runtime. Without this step then the
# libraries won't be found. You might not notice it if you have the libraries
# installed in your system. A lot of systems will have the Qt libs installed,
# but they might be a different version. It "might" work, it might cause subtle
# bugs, or it might not work at all.
#
# It's possible there's no current rpath for a particular library. It's also
# possible that the library has other dependencies in its existing rpath. So
# updating the rpath is non-trivial and it's a real shame that qmake doesn't
# do this for us.
#
# To patch the libraries `readelf` and `patchelf` tools are used.
#
# If the libraries' rpath isn't set correctly then LD_LIBRARY_PATH would need
# to be used, like such:
# LD_LIBRARY_PATH=./Qt/libs ./QGroundControl
#
# In addition, Qt will sometimes want to reference its own directory to find
# certain resources. This installs a file, qt.conf, which tells Qt where its
# installation is at.
# Without the qt.conf file, you would need to tell Qt where to find certain
# files, particularly for QML, like such:
# QML2_IMPORT_PATH=./Qt/qml QT_PLUGIN_PATH=./Qt/plugins ./QGroundControl
#

# -e: stop on error
# -u: undefined variable use is an error
# -o pipefail: if any part of a pipeline fails, then the whole pipeline fails.
set -euo pipefail

# To set these arguments, set them as an environment variable. For example:
# QTDIR=/opt/qgc-deploy/Qt RPATHDIR=/opt/qgc-deploy/Qt/libs QTCONF_PATH=/opt/qgc-deploy/qt.conf ./linux-post-link.sh
: "${QTDIR:=./Qt}"
: "${RPATHDIR:="${QTDIR}/libs"}"
: "${QTCONF_PATH:=./qt.conf}"

# find:
#    type f (files)
#    that end with '.so'
#    or that end with '.so.5'
#    and are executable
#    silence stderr (find will complain if it doesn't have permission to traverse)
find "${QTDIR}" \
    -type f \
    -iname '*.so' \
    -o -iname '*.so.5' \
    -executable \
    2>/dev/null |
while IFS='' read -r library; do
    # Get the library's current RPATH (RUNPATH)
    # Example output of `readelf readelf -d ./build/build-qgroundcontrol-Desktop_Qt_5_15_2_GCC_64bit-Debug/staging/QGroundControl`:
    #  0x000000000000001d (RUNPATH)            Library runpath: [$ORIGIN/Qt/libs:/home/kbennett/storage/Qt/5.15.2/gcc_64/lib]
    #
    # If you find a better way to do this, please fix.
    set +o pipefail
    current_rpath="$(
        # read the library, parsing its header
        # search for the RUNPATH field
        # filter out the human-readable text to leave only the RUNPATH value
        readelf -d "${library}" |
        grep -P '^ 0x[0-9a-f]+ +\(RUNPATH\) ' |
        sed -r 's/^ 0x[0-9a-f]+ +\(RUNPATH\) +Library runpath: \[(.*)\]$/\1/g'
    )"
    set -o pipefail

    # Get the directory containing the library
    library_dir="$(dirname "${library}")"

    # Get the relative path from the library's directory to the Qt/libs directory.
    our_rpath="$(realpath --relative-to "${library_dir}" "${RPATHDIR}")"

    # Calculate a new rpath with our library's rpath prefixed.
    # Note: '$ORIGIN' must not be expanded by the shell!
    # shellcheck disable=SC2016
    new_rpath='$ORIGIN/'"${our_rpath}"

    # If the library already had an rpath, then prefix ours to it.
    if [ -n "${current_rpath}" ]; then
        new_rpath="${new_rpath}:${current_rpath}"
    fi

    # patch the library's rpath
    patchelf --set-rpath "${new_rpath}" "${library}"
done

# Create a qt.conf file
# https://doc.qt.io/qt-5/qt-conf.html
cat <<EOF > "${QTCONF_PATH}"
[Paths]
Prefix=./Qt
Libraries=libs
EOF
