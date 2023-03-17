build_lib(
    LIBNAME videoconf
    SOURCE_FILES
        model/vca_server.cc
        model/vca_client.cc
        model/prot-header.cc
    HEADER_FILES
        model/vca_server.h
        model/vca_client.h
        model/prot-header.h
)