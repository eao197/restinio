/*
	restinio
*/

#include <catch2/catch.hpp>

#include <restinio/helpers/http_field_parser.hpp>

namespace hfp = restinio::http_field_parser;

struct simple_setter_t
{
	template< typename T >
	void operator()( T & to, T && from ) const noexcept(noexcept(to=from))
	{
		to = std::forward<T>(from);
	}
};

struct media_type_t
{
	std::string m_type;
	std::string m_subtype;

	static void
	type( media_type_t & to, std::string && what )
	{
		to.m_type = std::move(what);
	}

	static void
	subtype( media_type_t & to, std::string && what )
	{
		to.m_subtype = std::move(what);
	}
};

struct parameter_t
{
	std::string m_name;
	std::string m_value;

	static void
	name( parameter_t & to, std::string && what )
	{
		to.m_name = std::move(what);
	}

	static void
	value( parameter_t & to, std::string && what )
	{
		to.m_value = std::move(what);
	}
};

struct content_type_parsed_value_t
{
	media_type_t m_media_type;
	std::vector< parameter_t > m_parameters;

	static void
	type( content_type_parsed_value_t & to, std::string && what )
	{
		media_type_t::type( to.m_media_type, std::move(what) );
	}

	static void
	subtype( content_type_parsed_value_t & to, std::string && what )
	{
		media_type_t::subtype( to.m_media_type, std::move(what) );
	}

	static void
	parameters( 
		content_type_parsed_value_t & to,
		std::vector< parameter_t > && params )
	{
		to.m_parameters = std::move(params);
	}
};

TEST_CASE( "Simple" , "[simple]" )
{
	const char src[] = R"(multipart/form-data)";

	const auto result = hfp::try_parse_field_value< media_type_t >( src,
			hfp::rfc::token( media_type_t::type ),
			hfp::rfc::delimiter( '/' ),
			hfp::rfc::token( media_type_t::subtype ) );

	REQUIRE( result.first );
	REQUIRE( "multipart" == result.second.m_type );
	REQUIRE( "form-data" == result.second.m_subtype );
}

TEST_CASE( "Simple content_type value", "[simple][content-type]" )
{
	const char src[] = R"(multipart/form-data; boundary=---12345)";

	const auto result = hfp::try_parse_field_value< content_type_parsed_value_t >(
			src,
			hfp::rfc::token( content_type_parsed_value_t::type ),
			hfp::rfc::delimiter( '/' ),
			hfp::rfc::token( content_type_parsed_value_t::subtype ),
			hfp::any_occurences_of< std::vector< parameter_t > >(
					content_type_parsed_value_t::parameters,
					hfp::rfc::semicolon(),
					hfp::rfc::ows(),
					hfp::rfc::token( parameter_t::name ),
					hfp::rfc::delimiter( '=' ),
					hfp::rfc::token( parameter_t::value ) )
			);

	REQUIRE( result.first );
	REQUIRE( "multipart" == result.second.m_media_type.m_type );
	REQUIRE( "form-data" == result.second.m_media_type.m_subtype );

	REQUIRE( 1u == result.second.m_parameters.size() );
	REQUIRE( "boundary" == result.second.m_parameters[0].m_name );
	REQUIRE( "---12345" == result.second.m_parameters[0].m_value );
}

TEST_CASE( "Simple content_type value (two-params)", "[simple][content-type][two-params]" )
{
	const char src[] = R"(multipart/form-data; boundary=---12345; another=value)";

	const auto result = hfp::try_parse_field_value< content_type_parsed_value_t >(
			src,
			hfp::rfc::token( content_type_parsed_value_t::type ),
			hfp::rfc::delimiter( '/' ),
			hfp::rfc::token( content_type_parsed_value_t::subtype ),
			hfp::any_occurences_of< std::vector< parameter_t > >(
					content_type_parsed_value_t::parameters,
					hfp::rfc::semicolon(),
					hfp::rfc::ows(),
					hfp::rfc::token( parameter_t::name ),
					hfp::rfc::delimiter( '=' ),
					hfp::rfc::token( parameter_t::value ) )
			);

	REQUIRE( result.first );
	REQUIRE( "multipart" == result.second.m_media_type.m_type );
	REQUIRE( "form-data" == result.second.m_media_type.m_subtype );

	REQUIRE( 2u == result.second.m_parameters.size() );
	REQUIRE( "boundary" == result.second.m_parameters[0].m_name );
	REQUIRE( "---12345" == result.second.m_parameters[0].m_value );
	REQUIRE( "another" == result.second.m_parameters[1].m_name );
	REQUIRE( "value" == result.second.m_parameters[1].m_value );
}

TEST_CASE( "Simple content_type value (trailing spaces)", "[simple][content-type][trailing-spaces]" )
{
	const char src[] = R"(multipart/form-data; boundary=---12345  )";

	const auto result = hfp::try_parse_field_value< content_type_parsed_value_t >(
			src,
			hfp::rfc::token( content_type_parsed_value_t::type ),
			hfp::rfc::delimiter( '/' ),
			hfp::rfc::token( content_type_parsed_value_t::subtype ),
			hfp::any_occurences_of< std::vector< parameter_t > >(
					content_type_parsed_value_t::parameters,
					hfp::rfc::semicolon(),
					hfp::rfc::ows(),
					hfp::rfc::token( parameter_t::name ),
					hfp::rfc::delimiter( '=' ),
					hfp::rfc::token( parameter_t::value ) )
			);

	REQUIRE( result.first );
	REQUIRE( "multipart" == result.second.m_media_type.m_type );
	REQUIRE( "form-data" == result.second.m_media_type.m_subtype );

	REQUIRE( 1u == result.second.m_parameters.size() );
	REQUIRE( "boundary" == result.second.m_parameters[0].m_name );
	REQUIRE( "---12345" == result.second.m_parameters[0].m_value );
}

TEST_CASE( "Simple content_type value (trailing garbage)", "[simple][content-type][trailing-garbage]" )
{
	const char src[] = R"(multipart/form-data; boundary=---12345; bla-bla-bla)";

	const auto result = hfp::try_parse_field_value< content_type_parsed_value_t >(
			src,
			hfp::rfc::token( content_type_parsed_value_t::type ),
			hfp::rfc::delimiter( '/' ),
			hfp::rfc::token( content_type_parsed_value_t::subtype ),
			hfp::any_occurences_of< std::vector< parameter_t > >(
					content_type_parsed_value_t::parameters,
					hfp::rfc::semicolon(),
					hfp::rfc::ows(),
					hfp::rfc::token( parameter_t::name ),
					hfp::rfc::delimiter( '=' ),
					hfp::rfc::token( parameter_t::value ) )
			);

	REQUIRE( !result.first );
}
