file(GLOB SCRIPTS_SRC
        ${CMAKE_SOURCE_DIR}/scripts/asn1_compiler.c
        ${CMAKE_SOURCE_DIR}/scripts/selinux/genheaders/genheaders.c
        ${CMAKE_SOURCE_DIR}/scripts/selinux/mdp/mdp.c
        ${CMAKE_SOURCE_DIR}/scripts/mod/mk_elfconfig.c
        ${CMAKE_SOURCE_DIR}/scripts/mod/devicetable-offsets.c
        ${CMAKE_SOURCE_DIR}/scripts/mod/empty.c
        ${CMAKE_SOURCE_DIR}/scripts/mod/symsearch.c
        ${CMAKE_SOURCE_DIR}/scripts/mod/modpost.c
        ${CMAKE_SOURCE_DIR}/scripts/mod/sumversion.c
        ${CMAKE_SOURCE_DIR}/scripts/mod/file2alias.c
        ${CMAKE_SOURCE_DIR}/scripts/kallsyms.c
        ${CMAKE_SOURCE_DIR}/scripts/sorttable.c
        ${CMAKE_SOURCE_DIR}/scripts/basic/fixdep.c
)