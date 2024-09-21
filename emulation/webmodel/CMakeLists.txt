build_lib(
    LIBNAME webmodel
    SOURCE_FILES
        model/web_server.cc
        model/web_client.cc
        model/prot-header.cc
    HEADER_FILES
        model/web_server.h
        model/web_client.h
        model/prot-header.h
)