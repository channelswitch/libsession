. ./config
exec 3>libsession.pc
printf 'prefix=%s\n' "$prefix" >&3
printf 'exec_prefix=${prefix}\n' >&3
printf 'includedir=${prefix}/include\n' >&3
printf 'libdir=${exec_prefix}/lib/%s\n' "$target" >&3
printf '\n' >&3
printf 'Name: libsession\n' >&3
printf 'Description: The library for sessions\n' >&3
printf 'Version: 1.0.0\n' >&3
printf 'Cflags: -I${includedir}/session\n' >&3
printf 'Libs: -L${libdir} -lsession\n' >&3
exec 3>&-
