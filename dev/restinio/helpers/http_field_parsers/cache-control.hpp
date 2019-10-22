/*
 * RESTinio
 */

/*!
 * @file
 * @brief Stuff related to value of Cache-Control HTTP-field.
 *
 * @since v.0.6.1
 */

#pragma once

#include <restinio/helpers/http_field_parsers/basics.hpp>

namespace restinio
{

namespace http_field_parsers
{

//
// cache_control_value_t
//
struct cache_control_value_t
{
	using directive_t = std::pair<
			std::string,
			restinio::optional_t<std::string> >;

	using directive_container_t = std::map<
			std::string, restinio::optional_t<std::string> >;

	directive_container_t m_directives;

	static auto
	make_parser()
	{
		return produce< cache_control_value_t >(
			non_empty_comma_separated_list_producer< directive_container_t >(
				produce< directive_t >(
					token_producer() >> to_lower() >> &directive_t::first,
					maybe(
						symbol('='),
						alternatives(
							token_producer() >> &directive_t::second,
							quoted_string_producer() >> &directive_t::second
						)
					)
				)
			) >> &cache_control_value_t::m_directives
		);
	}

	static std::pair< bool, cache_control_value_t >
	try_parse( string_view_t what )
	{
		return restinio::easy_parser::try_parse( what, make_parser() );
	}
};

} /* namespace http_field_parsers */

} /* namespace restinio */
