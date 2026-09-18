# 清单驱动的测试可执行目标生成（R-08 / F-18）。
#
# 【为什么要有它】根 CMakeLists.txt 里有 171 个手写 add_executable 块，实测其中 123 个形状
# 完全一致：单源文件 + 单链接库 + 至多两种固定的额外设置。手写的代价不在写，在于
# CMakeLists.txt 是本仓常驻的合并冲突点（AGENTS.md 记录 MEMFLOW 的冲突面就含它）——
# 每个专项加测试都改同一处，而那种冲突长得和内容冲突一模一样，得逐块读完才能判是哪种。
# 收拢成一行一个之后，冲突退化成行级，git 多数情况下能自己合。
#
# 【为什么本函数不注册 add_test】三条实测理由，任一条单独成立即足够：
#   1. add_test 并非都是简单形式。例如
#      add_test(NAME unicode_path_contract_tests COMMAND unicode_path_contract_tests ${CMAKE_SOURCE_DIR})
#      带额外参数。若本函数也注册，这类目标会被【重复注册】。
#   2. 有 21 条 set_tests_properties 引用具体测试名，顺序敏感。
#   3. add_test 行【本来就是一行】——它从来不是冲突源；6~8 行的 add_executable 块才是。
#      把它挪进来不减冲突面，只增加改变 ctest 执行顺序的风险。
# 因此本函数只建可执行文件，add_test 一律保留在原处、原样不动。
#
# 【本模块不改变任何目标的行为】生成的 add_executable / target_link_libraries /
# target_compile_definitions / target_compile_options 与原手写块逐字等价，且因为是
# 原地替换，目标的定义顺序也不变——set_target_properties 与 add_dependencies 的引用照旧成立。
# 判据：ctest -N 的测试名集合在改动前后【完全相同】，且全量回归的失败集合不变。
#
# 【什么不该用它】多源文件、多条链接语句、带 target_include_directories 或其他
# target_* 设置的目标一律保留手写。宁可少转，也不要把一个带特殊设置的目标转成不带的——
# 那种错误会编过、会跑绿，只是少了一层检查。

# 生成一个「单源文件 + 单链接库」的测试可执行目标。
#
#   slicesoft_add_test_executable(
#       NAME  foo_unit_tests                 # 目标名
#       SRC   tests/unit/foo/Main.cpp        # 唯一的源文件
#       LIB   slicer_core                    # 唯一的 PRIVATE 链接库
#       [SOURCE_DIR_DEFINE]                  # 追加 SLICESOFT_SOURCE_DIR 定义
#       [STRICT_WARNINGS])                   # 追加 /W4 /WX
function(slicesoft_add_test_executable)
    set(options SOURCE_DIR_DEFINE STRICT_WARNINGS)
    set(oneValueArgs NAME SRC LIB)
    cmake_parse_arguments(SATE "${options}" "${oneValueArgs}" "" ${ARGN})

    if(NOT SATE_NAME)
        message(FATAL_ERROR "slicesoft_add_test_executable: NAME is required")
    endif()
    if(NOT SATE_SRC)
        message(FATAL_ERROR "slicesoft_add_test_executable(${SATE_NAME}): SRC is required")
    endif()
    if(NOT SATE_LIB)
        message(FATAL_ERROR "slicesoft_add_test_executable(${SATE_NAME}): LIB is required")
    endif()
    # 多余参数一律报错而不是忽略：关键字拼错（STRICT_WARNING 少个 S）会静默降级成
    # 「多余参数」，后果是某个目标少了 /W4 /WX 却照常编过，而没有任何东西会说。
    if(SATE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "slicesoft_add_test_executable(${SATE_NAME}): unexpected arguments: "
            "${SATE_UNPARSED_ARGUMENTS}")
    endif()

    add_executable(${SATE_NAME} ${SATE_SRC})
    target_link_libraries(${SATE_NAME} PRIVATE ${SATE_LIB})

    if(SATE_SOURCE_DIR_DEFINE)
        target_compile_definitions(${SATE_NAME}
            PRIVATE SLICESOFT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    # MSVC 守卫是必须的、不是多余的：仓库里这两个开关一律写成
    #     if(MSVC)
    #         target_compile_options(<name> PRIVATE /W4 /WX)
    #     endif()
    # 少了守卫就不是「与原手写块逐字等价」，换编译器时会传一个它不认识的开关。
    if(SATE_STRICT_WARNINGS AND MSVC)
        target_compile_options(${SATE_NAME} PRIVATE /W4 /WX)
    endif()
endfunction()
