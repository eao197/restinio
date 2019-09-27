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

#include <restinio/utils/metaprogramming.hpp>

#include <restinio/string_view.hpp>

#include <type_traits>
#include <tuple>

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
	using value_type = void;

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

template<std::size_t Index, typename Result>
void
parse_next(
	Result & result,
	source_t & source,
	const char separator,
	expect_t && what ) noexcept
{
	std::get<0>(result) = what.try_parse( source, separator );
}

//
// any_value_t
//
class any_value_t
{
public:
	using value_type = std::string;

	RESTINIO_NODISCARD
	bool
	try_parse( source_t & from, const char separator, value_type & collector )
	{
		for( auto ch = from.getch();
				!ch.m_eof && ch.m_ch != separator;
				ch = from.getch() )
		{
			collector += ch.m_ch;
		}

		return true;
	}
};

template<std::size_t Index, typename Result>
void
parse_next(
	Result & result,
	source_t & source,
	const char separator,
	any_value_t && what )
{
	any_value_t::value_type & collector = std::get<Index>(result);
	std::get<0>(result) = what.try_parse( source, separator, collector );
}

//
// name_value_t
//
class name_value_t
{
public:
	using value_type = std::string;

private:
	const string_view_t m_name;

	RESTINIO_NODISCARD
	bool
	try_parse_unquoted_value(
		source_t & from,
		const char separator,
		value_type & collector )
	{
		character_t ch;
		for( ch = from.getch();
				!ch.m_eof &&
				!is_space(ch.m_ch) &&
				separator != ch.m_ch;
				ch = from.getch() )
		{
			collector += ch.m_ch;
		}

		if( !ch.m_eof && separator != ch.m_ch )
			return source_t::try_skip_trailing_spaces_and_separator(
					from,
					separator );

		return true;
	}

	RESTINIO_NODISCARD
	bool
	try_parse_quoted_value(
		source_t & from,
		const char separator,
		value_type & collector )
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
				collector += next.m_ch;
			}
			else
				collector += ch.m_ch;
		}

		// All trailing spaces and separator should be skipped.
		return source_t::try_skip_trailing_spaces_and_separator(
				from,
				separator );
	}

public:
	explicit name_value_t( string_view_t name ) noexcept
		: m_name{ name }
	{}

	RESTINIO_NODISCARD
	bool
	try_parse( source_t & from, const char separator, value_type & collector )
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
			return try_parse_unquoted_value( from, separator, collector );
		}
		else
			return try_parse_quoted_value( from, separator, collector );
	}
};

template<std::size_t Index, typename Result>
void
parse_next(
	Result & result,
	source_t & source,
	const char separator,
	name_value_t && what )
{
	any_value_t::value_type & collector = std::get<Index>(result);
	std::get<0>(result) = what.try_parse( source, separator, collector );
}

//
// field_name_t
//
class field_name_t
{
	const string_view_t m_name;

public:
	using value_type = void;

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

template<std::size_t Index, typename Result>
void
parse_next(
	Result & result,
	source_t & source,
	const char /*separator*/,
	field_name_t && what ) noexcept
{
	std::get<0>(result) = what.try_parse( source );
}

namespace meta {

namespace mp = restinio::utils::metaprogramming;

template<typename T>
using to_tuple_t = mp::rename_t<T, std::tuple>;

template<typename T>
struct is_void_value_type
{
	static constexpr bool value =
			std::is_same<void, typename T::value_type>::value;
};

template<typename T>
struct detect_increment
{
	static constexpr std::size_t value =
			is_void_value_type<T>::value ? 0u : 1u;
};

template<typename T>
struct type_list_maker;

template<template<class...> class L>
struct type_list_maker< L<> >
{
	using type = mp::type_list<>;
};

template<
	template<class...> class L,
	typename... Rest>
struct type_list_maker< L<Rest...> >
	: type_list_maker< mp::tail_of_t<Rest...> >
{
	using base_type = type_list_maker< mp::tail_of_t<Rest...> >;
	using T = mp::head_of_t<Rest...>;

	using type = typename std::conditional<
			is_void_value_type<T>::value,
			typename base_type::type,
			mp::put_front_t<typename T::value_type, typename base_type::type>
		>::type;
};

template<typename... Rest>
using make_type_list_t =
		typename type_list_maker< mp::type_list<Rest...> >::type;

template<typename... Fragments>
using result_type_detector_t =
		to_tuple_t<
				mp::put_front_t<
						bool,
						make_type_list_t<Fragments...>
				>
		>;

} /* namespace meta */

//
// try_parse_impl
//
template< std::size_t Index, typename R, typename H >
void
try_parse_impl( R & result, source_t & from, const char separator, H && what )
{
	parse_next<Index>( result, from, separator, std::forward<H>(what) );
}

template< std::size_t Index, typename R, typename H, typename ...Tail >
void
try_parse_impl(
	R & result,
	source_t & from,
	const char separator,
	H && what,
	Tail && ...tail )
{
	parse_next<Index>( result, from, separator, std::forward<H>(what) );
	if( std::get<0>(result) )
		try_parse_impl<Index + meta::detect_increment<H>::value>(
				result, from, separator, std::forward<Tail>(tail)... );
}

} /* namespace impl */

//
// try_parse_field_value
//
template< typename ...Fragments >
RESTINIO_NODISCARD
auto
try_parse_field_value(
	string_view_t from,
	const char separator,
	Fragments && ...fragments )
{
	impl::source_t source{ from };

	using result_tuple_t = impl::meta::result_type_detector_t<Fragments...>;
	result_tuple_t result;

	std::get<0>(result) = false;

	// Use index 1 because index 0 is used by boolean flag.
	impl::try_parse_impl<1>(
			result,
			source,
			separator,
			std::forward<Fragments>(fragments)... );

	return result;
}

//
// try_parse_whole_field
//
template< typename ...Fragments >
RESTINIO_NODISCARD
auto
try_parse_whole_field(
	string_view_t from,
	string_view_t field_name,
	const char separator,
	Fragments && ...fragments )
{
	impl::source_t source{ from };

	using result_tuple_t = impl::meta::result_type_detector_t<Fragments...>;
	result_tuple_t result;

	std::get<0>(result) = false;

	// Use index 1 because index 0 is used by boolean flag.
	impl::try_parse_impl<1>(
			result,
			source,
			separator,
			impl::field_name_t{ field_name },
			std::forward<Fragments>(fragments)... );

	return result;
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
any() noexcept
{
	return impl::any_value_t{};
}

//
// name_value_t
//
RESTINIO_NODISCARD
inline auto
name_value(
	string_view_t what ) noexcept
{
	return impl::name_value_t{ what };
}

} /* namespace http_field_parser */

} /* namespace restinio */

