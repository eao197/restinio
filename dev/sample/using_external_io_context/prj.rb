require 'mxx_ru/cpp'

MxxRu::Cpp::exe_target {

  target 'sample.using_external_io_context'

  required_prj 'asio_mxxru/prj.rb'
  required_prj 'nodejs/http_parser_mxxru/prj.rb'
  required_prj 'fmt_mxxru/prj.rb'
  required_prj 'restinio/platform_specific_libs.rb'

  cpp_source 'main.cpp'
}