/*
 * RESTinio
 */

/*!
 * @file
 * @brief Stuff related to value of Accept HTTP-field.
 *
 * @since v.0.6.1
 */

#pragma once

#include <restinio/helpers/http_field_parsers/media-type.hpp>

namespace restinio
{

namespace http_field_parsers
{

//
// accept_value_t
//
struct accept_value_t
{
	struct item_t
	{
		using accept_ext_t = parameter_with_optional_value_t;

		using accept_ext_container_t = parameter_with_optional_value_container_t;

		media_type_value_t m_media_type;
		restinio::optional_t< qvalue_t > m_weight;
		accept_ext_container_t m_accept_params;
	};

	using item_container_t = std::vector< item_t >;

	item_container_t m_items;

	static auto
	make_parser()
	{
		const auto media_type = media_type_value_t::make_weight_aware_parser();

		return produce< accept_value_t >(
			maybe_empty_comma_separated_list_producer< item_container_t >(
				produce< item_t >(
					media_type >> &item_t::m_media_type,
					maybe(
						weight_producer() >> &item_t::m_weight,
						params_with_opt_value_producer() >> &item_t::m_accept_params
					)
				)
			) >> &accept_value_t::m_items
		);
	}

	static std::pair< bool, accept_value_t >
	try_parse( string_view_t what )
	{
		return restinio::easy_parser::try_parse( what, make_parser() );
	}
};

} /* namespace http_field_parsers */

} /* namespace restinio */
