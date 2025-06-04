add_executable(QtWebDAV-example
        main.cpp
        qexample.cpp
        qexample.h
        )

target_link_libraries(QtWebDAV-example PRIVATE QtWebDAV)
set_target_properties(QtWebDAV-example  PROPERTIES AUTOMOC ON)