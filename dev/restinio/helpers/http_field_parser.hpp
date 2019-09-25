/*
 * RESTinio
 */

/*!
 * @file
 * @brief Utilities for parsing values of http-fields
 *
 * @since v.0.6.1
 */

#pragma once

#include <restinio/impl/to_lower_lut.hpp>

#include <restinio/string_view.hpp>

namespace restinio
{

namespace http_field_parser
{

namespace impl
{

//
// character_t
//
struct character_t
{
	bool m_eof;
	char m_ch;
};

RESTINIO_NODISCARD
inline bool
operator==( const character_t & a, const character_t & b ) noexcept
{
	return (a.m_eof == b.m_eof && a.m_ch == b.m_ch);
}

RESTINIO_NODISCARD
inline bool
operator!=( const character_t & a, const character_t & b ) noexcept
{
	return (a.m_eof != b.m_eof || a.m_ch != b.m_ch);
}

//
// is_space
//
RESTINIO_NODISCARD
inline bool
is_space( const char ch ) noexcept { return ch == ' ' || ch == '\x09'; }

//
// source_t
//
class source_t
{
	const string_view_t m_data;
	string_view_t::size_type m_index{};

public:
	explicit source_t( string_view_t data ) noexcept : m_data{ data } {}

	RESTINIO_NODISCARD
	character_t
	getch() noexcept
	{
		if( m_index < m_data.size() )
		{
			return {false, m_data[ m_index++ ]};
		}
		else
			return {true, 0};
	}

	void
	putback() noexcept
	{
		if( m_index )
			--m_index;
	}

	RESTINIO_NODISCARD
	static bool
	try_skip_leading_spaces( source_t & from ) noexcept
	{
		for( auto ch = from.getch();; ch = from.getch() )
		{
			if( ch.m_eof ) return false;
			else if( !is_space( ch.m_ch ) ) {
				from.putback();
				break;
			}
		}
		return true;
	}

	RESTINIO_NODISCARD
	static bool
	try_skip_value(
		source_t & from,
		const string_view_t & value ) noexcept
	{
		const unsigned char * const table =
				restinio::impl::to_lower_lut< unsigned char >();
		const auto uchar_from_table = [table](char c) {
			return table[ static_cast<unsigned char>(c) ];
		};

		for( const auto c : value )
		{
			const auto ch = from.getch();
			if( ch.m_eof || uchar_from_table(c) != uchar_from_table(ch.m_ch) )
				return false;
		}
		return true;
	}

	RESTINIO_NODISCARD
	static bool
	try_skip_trailing_spaces_and_separator(
		source_t & from,
		const char separator ) noexcept
	{
		for( auto ch = from.getch();; ch = from.getch() )
		{
			if( ch.m_eof )
				return true;
			else if( separator == ch.m_ch )
				return true;
			else if( !is_space( ch.m_ch ) )
				return false;
		}

		return true;
	}
};

//
// expect_t
//
class expect_t
{
	const string_view_t m_value;

public:
	explicit expect_t( string_view_t value ) noexcept : m_value{ value } {}

	RESTINIO_NODISCARD
	bool
	try_parse( source_t & from, const char separator ) const noexcept
	{
		if( !source_t::try_skip_leading_spaces( from ) )
			return false;

		// Compare the main value.
		if( !source_t::try_skip_value( from, m_value ) )
			return false;

		return source_t::try_skip_trailing_spaces_and_separator(
				from,
				separator );
	}
};

RESTINIO_NODISCARD
inline bool
parse_next(
	source_t & source,
	const char separator,
	expect_t && what ) noexcept
{
	return what.try_parse( source, separator );
}

//
// any_value_t
//
class any_value_t
{
	std::string & m_collector;

public:
	explicit any_value_t( std::string & collector )
		: m_collector{ collector }
	{}

	RESTINIO_NODISCARD
	bool
	try_parse( source_t & from, const char separator )
	{
		for( auto ch = from.getch();
				!ch.m_eof && ch.m_ch != separator;
				ch = from.getch() )
		{
			m_collector += ch.m_ch;
		}

		return true;
	}
};

RESTINIO_NODISCARD
inline bool
parse_next(
	source_t & source,
	const char separator,
	any_value_t && what )
{
	return what.try_parse( source, separator );
}

//
// name_value_t
//
class name_value_t
{
	const string_view_t m_name;
	std::string & m_collector;

	RESTINIO_NODISCARD
	bool
	try_parse_unquoted_value( source_t & from, const char separator )
	{
		character_t ch;
		for( ch = from.getch();
				!ch.m_eof &&
				!is_space(ch.m_ch) &&
				separator != ch.m_ch;
				ch = from.getch() )
		{
			m_collector += ch.m_ch;
		}

		if( !ch.m_eof && separator != ch.m_ch )
			return source_t::try_skip_trailing_spaces_and_separator(
					from,
					separator );

		return true;
	}

	RESTINIO_NODISCARD
	bool
	try_parse_quoted_value( source_t & from, const char separator )
	{
		for(;;)
		{
			const auto ch = from.getch();
			if( ch.m_eof )
				return false;
			else if( '"' == ch.m_ch )
				break;
			else if( '\\' == ch.m_ch ) {
				const auto next = from.getch();
				if( next.m_eof )
					return false;
				m_collector += next.m_ch;
			}
			else
				m_collector += ch.m_ch;
		}

		// All trailing spaces and separator should be skipped.
		return source_t::try_skip_trailing_spaces_and_separator(
				from,
				separator );
	}

public:
	explicit name_value_t(
		string_view_t name,
		std::string & collector ) noexcept
		: m_name{ name }
		, m_collector{ collector }
	{}

	RESTINIO_NODISCARD
	bool
	try_parse( source_t & from, const char separator )
	{
		if( !source_t::try_skip_leading_spaces( from ) )
			return false;

		// Compare the main value.
		if( !source_t::try_skip_value( from, m_name ) )
			return false;

		if( character_t{false, '='} != from.getch() )
			return false;

		const auto first_value_ch = from.getch();
		if( first_value_ch.m_eof )
			return false;

		if( '"' != first_value_ch.m_ch )
		{
			from.putback();
			return try_parse_unquoted_value( from, separator );
		}
		else
			return try_parse_quoted_value( from, separator );
	}
};

RESTINIO_NODISCARD
inline bool
parse_next(
	source_t & source,
	const char separator,
	name_value_t && what )
{
	return what.try_parse( source, separator );
}

//
// field_name_t
//
class field_name_t
{
	const string_view_t m_name;

public:
	explicit field_name_t( string_view_t name ) noexcept : m_name{ name } {}

	RESTINIO_NODISCARD
	bool
	try_parse( source_t & from ) const noexcept
	{
		// There should not be leading spaces.
		// Compare the main value.
		if( !source_t::try_skip_value( from, m_name ) )
			return false;

		// The next char is expected to be ':'.
		const auto ch = from.getch();
		return !ch.m_eof && ch.m_ch == ':';
	}
};

RESTINIO_NODISCARD
inline bool
parse_next(
	source_t & source,
	const char separator,
	field_name_t && what ) noexcept
{
	return what.try_parse( source );
}

//
// try_parse_impl
//
template< typename H >
RESTINIO_NODISCARD
bool
try_parse_impl( source_t & from, const char separator, H && what )
{
	return parse_next( from, separator, std::forward<H>(what) );
}

template< typename H, typename ...Tail >
RESTINIO_NODISCARD
bool
try_parse_impl(
	source_t & from,
	const char separator,
	H && what,
	Tail && ...tail )
{
	auto r = parse_next( from, separator, std::forward<H>(what) );
	if( r )
		r = try_parse_impl( from, separator, std::forward<Tail>(tail)... );

	return r;
}

} /* namespace impl */

//
// try_parse_field_value
//
template< typename ...Fragments >
RESTINIO_NODISCARD
bool
try_parse_field_value(
	string_view_t from,
	const char separator,
	Fragments && ...fragments )
{
	impl::source_t source{ from };
	return impl::try_parse_impl(
			source,
			separator,
			std::forward<Fragments>(fragments)... );
}

//
// try_parse_whole_field
//
template< typename ...Fragments >
RESTINIO_NODISCARD
bool
try_parse_whole_field(
	string_view_t from,
	string_view_t field_name,
	const char separator,
	Fragments && ...fragments )
{
	impl::source_t source{ from };
	return impl::try_parse_impl(
			source,
			separator,
			impl::field_name_t{ field_name },
			std::forward<Fragments>(fragments)... );
}

//
// expect
//
RESTINIO_NODISCARD
inline auto
expect( string_view_t what ) noexcept
{
	return impl::expect_t{ what };
}

//
// any
//
RESTINIO_NODISCARD
inline auto
any( std::string & collector ) noexcept
{
	return impl::any_value_t{ collector };
}

//
// name_value_t
//
RESTINIO_NODISCARD
inline auto
name_value(
	string_view_t what,
	std::string & collector ) noexcept
{
	return impl::name_value_t{ what, collector };
}

} /* namespace http_field_parser */

} /* namespace restinio */

