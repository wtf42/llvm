if(WIN32 OR CYGWIN)
  set(LLVM_LINK_COMPONENTS Core Support)
endif()

add_llvm_loadable_module( LLVMInlineAssembly2
  InlineAssembly2.cpp

  DEPENDS
  intrinsics_gen
  )
