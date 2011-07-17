INCLUDE(FindJNI)
INCLUDE(FindJava)
INCLUDE_DIRECTORIES(simple_interface_generators/glue/java ${JNI_INCLUDE_DIRS})

SET(java_bridge_files)

IF (CHECK_BUILD AND NOT JNI_FOUND)
  MESSAGE(FATAL_ERROR "Java Native Interface libraries / includes were not found, but you have enabled Java support. To override the pre-build checks and manually fix any problems, pass -DCHECK_BUILD:BOOL=OFF to CMake.")
ENDIF()

SET(ALL_JAVA_FILES)
SET(ALL_JAVACLASS_FILES)

LIST(APPEND ALL_JAVA_FILES "simple_interface_generators/glue/java/pjm/XPCOMDerived.java")
LIST(APPEND ALL_JAVACLASS_FILES "javacp/pjm/XPCOMDerived.class")
LIST(APPEND ALL_JAVA_FILES "simple_interface_generators/glue/java/pjm/Reference.java")
LIST(APPEND ALL_JAVACLASS_FILES "javacp/pjm/Reference.class")

FOREACH(extension ${EXTENSION_LIST})
  FOREACH(idlname ${IDL_LIST_${extension}})
    SET(THESE_JAVA_FILES)
    FOREACH(iface ${${idlname}_INTERFACES})
      LIST(APPEND ALL_JAVA_FILES interfaces/${${idlname}_NAMESPACE}/${iface}.java)
      LIST(APPEND THESE_JAVA_FILES interfaces/${${idlname}_NAMESPACE}/${iface}.java)
      LIST(APPEND ALL_JAVACLASS_FILES javacp/${${idlname}_NAMESPACE}/${iface}.class)

      LIST(APPEND ALL_JAVA_FILES interfaces/pjm2pcm/${${idlname}_NAMESPACE}/${iface}.java)
      LIST(APPEND THESE_JAVA_FILES interfaces/pjm2pcm/${${idlname}_NAMESPACE}/${iface}.java)
      LIST(APPEND ALL_JAVACLASS_FILES javacp/pjm2pcm/${${idlname}_NAMESPACE}/${iface}.class)
    ENDFOREACH(iface)
    FOREACH(enum ${${idlname}_ENUMS})
      LIST(APPEND ALL_JAVA_FILES interfaces/${${idlname}_NAMESPACE}/${enum}.java)
      LIST(APPEND THESE_JAVA_FILES interfaces/${${idlname}_NAMESPACE}/${enum}.java)
      LIST(APPEND ALL_JAVACLASS_FILES javacp/${${idlname}_NAMESPACE}/${enum}.class)
    ENDFOREACH(enum)

    SET(idlpath "interfaces/${idlname}.idl")

    ADD_CUSTOM_COMMAND(OUTPUT ${THESE_JAVA_FILES} interfaces/p2j${idlname}.cpp interfaces/p2j${idlname}.hxx interfaces/j2p${idlname}.cpp interfaces/j2p${idlname}.hxx 
      COMMAND ${OMNIIDL} -bjava -Iinterfaces -p../simple_interface_generators/omniidl_be ../${idlpath}
      MAIN_DEPENDENCY ${idlpath} DEPENDS
      simple_interface_generators/omniidl_be/java/__init__.py
      simple_interface_generators/omniidl_be/java/interfacegen.py
      simple_interface_generators/omniidl_be/java/j2pcm.py
      simple_interface_generators/omniidl_be/java/jnutils.py
      simple_interface_generators/omniidl_be/java/nativeclassgen.py
      simple_interface_generators/omniidl_be/java/pcm2j.py
      WORKING_DIRECTORY interfaces VERBATIM)
    LIST(APPEND java_bridge_files interfaces/p2j${idlname}.cpp interfaces/j2p${idlname}.cpp)
  ENDFOREACH(idlname)
ENDFOREACH(extension)

FOREACH(bootstrap ${BOOTSTRAP_LIST})
  IF(NOT EXISTS "javagen/cellml_bootstrap/${bootstrap}.java")
    FILE(MAKE_DIRECTORY javagen/cellml_bootstrap)
    FILE(WRITE "javagen/cellml_bootstrap/${bootstrap}.java" "package cellml_bootstrap;\npublic class ${bootstrap}\n{\n  public static native ${BOOTSTRAP_${bootstrap}_IFACEMODULE}.${BOOTSTRAP_${bootstrap}_IFACE} ${BOOTSTRAP_${bootstrap}_METHOD}();\n};\n")
  ENDIF()
  IF(NOT EXISTS "javagen/${bootstrap}Java.cpp")
    FILE(WRITE "javagen/${bootstrap}Java.cpp" "#include <exception>\n#include \"pick-jni.h\"\n#include \"j2p${BOOTSTRAP_${bootstrap}_IDL}.hxx\"\n#include \"${BOOTSTRAP_${bootstrap}_HEADER}\"\nextern \"C\" { JWRAP_PUBLIC_PRE jobject Java_cellml_1bootstrap_${IFACE_JESCAPE}_${BOOTSTRAP_${bootstrap}_METHOD}(JNIEnv* env, jclass clazz) JWRAP_PUBLIC_POST; }\n\njobject\nJava_cellml_1bootstrap_${IFACE_JESCAPE}_${BOOTSTRAP_${bootstrap}_METHOD}(JNIEnv* env, jclass clazz)\n{\n  RETURN_INTO_OBJREF(b, iface::${BOOTSTRAP_${bootstrap}_IFACEMODULE}::${BOOTSTRAP_${bootstrap}_IFACE}, ${BOOTSTRAP_${bootstrap}_METHODCXX}());\n  return wrap_${BOOTSTRAP_${bootstrap}_IFACEMODULE}_${BOOTSTRAP_${bootstrap}_IFACE}(env, b);\n}\n")
    LIST(APPEND java_bridge_files javagen/${bootstrap}Java.cpp)
  ENDIF()
  LIST(APPEND ALL_JAVA_FILES_NODEPEND javagen/cellml_bootstrap/${bootstrap}.java)
  LIST(APPEND ALL_JAVACLASS_FILES javacp/cellml_bootstrap/${bootstrap}.class)
ENDFOREACH(bootstrap)

FILE(MAKE_DIRECTORY javacp)
ADD_CUSTOM_COMMAND(OUTPUT ${ALL_JAVACLASS_FILES}
  COMMAND ${Java_JAVAC_EXECUTABLE} ${ALL_JAVA_FILES} ${ALL_JAVA_FILES_NODEPEND} -d javacp
  DEPENDS ${ALL_JAVA_FILES})

ADD_CUSTOM_COMMAND(OUTPUT cellml.jar COMMAND ${Java_JAR_EXECUTABLE} cf cellml.jar -C javacp .
  DEPENDS ${ALL_JAVACLASS_FILES} VERBATIM)
ADD_CUSTOM_TARGET(JAVA_BUILD ALL DEPENDS cellml.jar)

ADD_LIBRARY(cellml_java_bridge MODULE ${java_bridge_files})