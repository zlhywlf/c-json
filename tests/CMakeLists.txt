file(GLOB_RECURSE files *Test.c)
file(GLOB_RECURSE  files2 ../src/*.c)
foreach(file ${files})
    string(REGEX REPLACE ".+/(.+)\\..*" "\\1" exe ${file})
    add_executable(${exe} ${file} ${files2})
    message("exe=${exe}  file=${file}")
endforeach()