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

#include <restinio/utils/tuple_algorithms.hpp>

#include <restinio/string_view.hpp>
#include <restinio/compiler_features.hpp>

#include <restinio/optional.hpp>

#include <iostream>
#include <limits>
#include <map>

namespace restinio
{

namespace http_field_parser
{

//FIXME: document this!
struct nothing_t {};

//FIXME: document this!
template< typename T >
struct default_container_adaptor;

template< typename T, typename... Args >
struct default_container_adaptor< std::vector< T, Args... > >
{
	using container_type = std::vector< T, Args... >;
	using value_type = typename container_type::value_type;

	static void
	store( container_type & to, value_type && what )
	{
		to.push_back( std::move(what) );
	}
};

template< typename K, typename V, typename... Args >
struct default_container_adaptor< std::map< K, V, Args... > >
{
	using container_type = std::map< K, V, Args... >;
	// NOTE: we can't use container_type::value_type here
	// because value_type for std::map is std::pair<const K, V>,
	// not just std::pair<K, V>,
	using value_type = std::pair<K, V>;

	static void
	store( container_type & to, value_type && what )
	{
		to.emplace( std::move(what) );
	}
};

template<>
struct default_container_adaptor< nothing_t >
{
	using container_type = nothing_t;
	using value_type = nothing_t;

	static void
	store( container_type &, value_type && ) noexcept {}
};

constexpr std::size_t N = std::numeric_limits<std::size_t>::max();

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

constexpr char SP = ' ';
constexpr char	HTAB = '\x09';

//
// is_space
//
RESTINIO_NODISCARD
inline constexpr bool
is_space( const char ch ) noexcept
{
	return ch == SP || ch == HTAB;
}

//
// is_vchar
//
RESTINIO_NODISCARD
inline constexpr bool
is_vchar( const char ch ) noexcept
{
	return (ch >= '\x41' && ch <= '\x5A') ||
			(ch >= '\x61' && ch <= '\x7A');
}

//
// is_obs_text
//
RESTINIO_NODISCARD
inline constexpr bool
is_obs_text( const char ch ) noexcept
{
	constexpr unsigned short left = 0x80u;
	constexpr unsigned short right = 0xFFu;

	const unsigned short t = static_cast<unsigned short>(
			static_cast<unsigned char>(ch));

	return (t >= left && t <= right);
}

//
// is_qdtext
//
RESTINIO_NODISCARD
inline constexpr bool
is_qdtext( const char ch ) noexcept
{
	return ch == SP ||
			ch == HTAB ||
			ch == '!' ||
			(ch >= '\x23' && ch <= '\x5B') ||
			(ch >= '\x5D' && ch <= '\x7E') ||
			is_obs_text( ch );
}

//
// is_digit
//
RESTINIO_NODISCARD
inline constexpr bool
is_digit( const char ch ) noexcept
{
	return (ch >= '0' && ch <= '9');
}

//
// source_t
//
class source_t
{
	const string_view_t m_data;
	string_view_t::size_type m_index{};

public:
	using position_t = string_view_t::size_type;

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
	position_t
	current_position() const noexcept
	{
		return m_index;
	}

	void
	backto( position_t pos ) noexcept
	{
		if( pos <= m_data.size() )
			m_index = pos;
	}

	RESTINIO_NODISCARD
	bool
	eof() const noexcept
	{
		return m_index >= m_data.size();
	}

//FIXME: this is debug method!
string_view_t
current_content() const noexcept
{
	if( m_index < m_data.size() )
		return m_data.substr( m_index );
	else
		return {"--EOF--"};
}

};

enum class construction_time_t
{
	compiletime,
	runtime
};

template< typename Target_Type, construction_time_t Construction_Time >
struct producer_tag
{
	using target_type = Target_Type;
	static constexpr construction_time_t construction_time = Construction_Time;
};

template< typename P >
class constexpr_constructible_value_producer_t
{
protected :
	P m_producer;

public :
	constexpr constexpr_constructible_value_producer_t( P && producer )
		:	m_producer{ std::move(producer) }
	{}
};

template< typename P >
class runtime_constructible_value_producer_t
{
protected :
	P m_producer;

public :
	runtime_constructible_value_producer_t( P && producer )
		:	m_producer{ std::move(producer) }
	{}
};

template< typename P, construction_time_t Construction_Time >
struct value_producer_base_selector_impl;

template< typename P >
struct value_producer_base_selector_impl<P, construction_time_t::compiletime>
{
	using type = constexpr_constructible_value_producer_t<P>;
};

template< typename P >
struct value_producer_base_selector_impl<P, construction_time_t::runtime>
{
	using type = runtime_constructible_value_producer_t<P>;
};

template< typename P >
using value_producer_base_selector_t =
	typename value_producer_base_selector_impl< P, P::construction_time >::type;

template< typename P >
class value_producer_t
	: protected value_producer_base_selector_t<P>
{
	using base_type = value_producer_base_selector_t<P>;

public :
	using base_type::base_type;

	P giveaway() noexcept(noexcept(P{std::move(std::declval<P>())}))
	{
		return std::move(this->m_producer);
	}

	RESTINIO_NODISCARD
	auto
	try_parse( source_t & source )
	{
		return this->m_producer.try_parse( source );
	}
};

template< typename Incoming_Type, typename Result_Type >
struct transformer_tag
{
	using incoming_type = Incoming_Type;
	using result_type = Result_Type;
};

template< typename T >
class value_transformer_t
{
	T m_transformer;

public :
	using incoming_type = typename T::incoming_type;
	using result_type = typename T::result_type;

	value_transformer_t(
		T && transformer )
		:	m_transformer{ std::move(transformer) }
	{}

	T giveaway() noexcept(noexcept(T{std::move(std::declval<T>())}))
	{
		return std::move(m_transformer);
	}

	RESTINIO_NODISCARD
	auto
	transform( incoming_type && input )
	{
		return m_transformer.transform( std::move(input) );
	}
};

template< typename Producer, typename Transformer >
class transformed_value_producer_t
	:	public producer_tag<
				typename Producer::target_type,
				Producer::construction_time >
{
	Producer m_producer;
	Transformer m_transformer;

public :
	using result_type = typename Transformer::result_type;

	transformed_value_producer_t(
		Producer && producer,
		Transformer && transformer )
		:	m_producer{ std::move(producer) }
		,	m_transformer{ std::move(transformer) }
	{}

	RESTINIO_NODISCARD
	auto
	try_parse( source_t & source )
	{
		std::pair< bool, result_type > result;
		
		auto producer_result = m_producer.try_parse( source );
		result.first = producer_result.first;
		if( producer_result.first )
		{
			result.second = m_transformer.transform(
					std::move(producer_result.second) );
		}

		return result;
	}
};

template< typename P, typename T >
auto
operator>>(
	value_producer_t<P> producer,
	value_transformer_t<T> transformer )
{
	using transformator_type = transformed_value_producer_t< P, T >;
	using result_type = value_producer_t< transformator_type >;

	return result_type{
			transformator_type{
					producer.giveaway(), transformer.giveaway()
			}
	};
};

template< typename C >
class value_consumer_t
{
	C m_consumer;

public :
	value_consumer_t( C && consumer ) : m_consumer{ std::move(consumer) } {}

	C giveaway() noexcept(noexcept(C{std::move(std::declval<C>())}))
	{
		return std::move(m_consumer);
	}

	template< typename Target_Type, typename Value >
	void
	consume( Target_Type & target, Value && value )
	{
		m_consumer.consume( target, std::forward<Value>(value) );
	}
};

template< typename P, typename C >
class clause_t
{
	P m_producer;
	C m_consumer;

public :
	clause_t( P && producer, C && consumer )
		:	m_producer{ std::move(producer) }
		,	m_consumer{ std::move(consumer) }
	{}

	template< typename Target_Type >
	bool
	try_process( source_t & from, Target_Type & target )
	{
		auto parse_result = m_producer.try_parse( from );
		if( parse_result.first )
		{
			m_consumer.consume( target, std::move(parse_result.second) );
			return true;
		}
		else
			return false;
	}
};

template< typename P, typename C >
RESTINIO_NODISCARD
clause_t< P, C >
operator>>( value_producer_t<P> producer, value_consumer_t<C> consumer )
{
	return { producer.giveaway(), consumer.giveaway() };
}

template< typename Target_Type, typename Clauses_Tuple >
class top_level_clause_t
{
	Clauses_Tuple m_clauses;

public :
	top_level_clause_t( Clauses_Tuple && clauses )
		:	m_clauses{ std::move(clauses) }
	{}

	RESTINIO_NODISCARD
	auto
	try_process( source_t & from )
	{
		std::pair< bool, Target_Type > result;

		if( restinio::utils::tuple_algorithms::all_of(
				m_clauses,
				[&from, &result]( auto && one_clause ) {
					return one_clause.try_process( from, result.second );
				} ) )
		{
			result.first = true;
		}
		else
			result.first = false;

		return result;
	}
};

RESTINIO_NODISCARD
bool
ensure_no_remaining_content(
	source_t & from )
{
	while( !from.eof() )
	{
		if( !is_space( from.getch().m_ch ) )
			return false;
	}

	return true;
}

//
// alternatives_t
//
template<
	typename Target_Type,
	typename Subitems_Tuple >
class alternatives_t
	: public producer_tag<Target_Type, construction_time_t::runtime>
{
	Subitems_Tuple m_subitems;

public :
	alternatives_t(
		Subitems_Tuple && subitems )
		:	m_subitems{ std::move(subitems) }
	{}

	RESTINIO_NODISCARD
	auto
	try_parse( source_t & from )
	{
		std::pair< bool, Target_Type > result;
		result.first = false;

		(void)restinio::utils::tuple_algorithms::any_of(
				m_subitems,
				[&from, &result]( auto && one_producer ) {
					const auto pos = from.current_position();
					result = one_producer.try_parse( from );
					if( !result.first )
						from.backto( pos );
					return result.first;
				} );

		return result;
	}
};

//
// produce_t
//
template<
	typename Target_Type,
	typename Subitems_Tuple >
class produce_t
	: public producer_tag<Target_Type, construction_time_t::runtime>
{
	Subitems_Tuple m_subitems;

public :
	produce_t(
		Subitems_Tuple && subitems )
		:	m_subitems{ std::move(subitems) }
	{}

	RESTINIO_NODISCARD
	auto
	try_parse( source_t & from )
	{
		std::pair< bool, Target_Type > result;

		const auto pos = from.current_position();

		result.first = restinio::utils::tuple_algorithms::all_of(
				m_subitems,
				[&from, &result]( auto && one_clause ) {
					return one_clause.try_process( from, result.second );
				} );

		if( !result.first )
			from.backto( pos );

		return result;
	}
};

//
// optional_producer_t
//
template<
	typename Target_Type,
	typename Subitems_Tuple >
class optional_producer_t
	: public producer_tag<Target_Type, construction_time_t::runtime>
{
	Subitems_Tuple m_subitems;

public :
	optional_producer_t(
		Subitems_Tuple && subitems )
		:	m_subitems{ std::move(subitems) }
	{}

	RESTINIO_NODISCARD
	auto
	try_parse( source_t & from )
	{
		Target_Type tmp_value;

		const auto pos = from.current_position();

		const bool parsed = restinio::utils::tuple_algorithms::all_of(
				m_subitems,
				[&from, &tmp_value]( auto && one_clause ) {
					return one_clause.try_process( from, tmp_value );
				} );

		if( !parsed )
			from.backto( pos );

		std::pair< bool, restinio::optional_t<Target_Type> > result;
		result.first = true;
		if( parsed )
			result.second = std::move(tmp_value);

		return result;
	}
};

//
// repeat_t
//
template<
	typename Container_Adaptor,
	typename Subitems_Tuple >
class repeat_t
	: public producer_tag<
			typename Container_Adaptor::container_type,
			construction_time_t::runtime>
{
	using container_type = typename Container_Adaptor::container_type;
	using value_type = typename Container_Adaptor::value_type;

	std::size_t m_min_occurences;
	std::size_t m_max_occurences;

	Subitems_Tuple m_subitems;

public :
	repeat_t(
		std::size_t min_occurences,
		std::size_t max_occurences,
		Subitems_Tuple && subitems )
		:	m_min_occurences{ min_occurences }
		,	m_max_occurences{ max_occurences }
		,	m_subitems{ std::move(subitems) }
	{}

	RESTINIO_NODISCARD
	auto
	try_parse( source_t & from )
	{
		std::pair< bool, container_type > result;

		std::size_t count{};
		bool failure_detected{ false };
		for(; !failure_detected && count != m_max_occurences; ++count )
		{
			value_type item;

			const auto pos = from.current_position();
			failure_detected = !restinio::utils::tuple_algorithms::all_of(
					m_subitems,
					[&from, &item]( auto && one_clause ) {
						return one_clause.try_process( from, item );
					} );

			if( !failure_detected )
			{
				// Another item successfully parsed and should be stored.
				Container_Adaptor::store( result.second, std::move(item) );
			}
			else
				from.backto( pos );
		}

		result.first = count >= m_min_occurences;

		return result;
	}
};

//
// one_or_more_of_t
//
template<
	typename Container_Adaptor,
	typename Producer >
class one_or_more_of_t
	: public producer_tag<
			typename Container_Adaptor::container_type,
			construction_time_t::runtime>
{
	using container_type = typename Container_Adaptor::container_type;
	using value_type = typename Container_Adaptor::value_type;

	value_producer_t<Producer> m_producer;

public :
	one_or_more_of_t( Producer && producer )
		:	m_producer{ std::move(producer) }
	{}

	RESTINIO_NODISCARD
	std::pair< bool, container_type >
	try_parse( source_t & from );
};

namespace rfc
{

class ows_t
	:	public producer_tag<
			restinio::optional_t<char>,
			construction_time_t::compiletime >
{
public :
	RESTINIO_NODISCARD
	auto
	try_parse(
		source_t & from ) const noexcept
	{
		std::pair< bool, optional_t<char> > result;

		std::size_t extracted_spaces{};
		for( auto ch = from.getch();
			!ch.m_eof && is_space(ch.m_ch);
			ch = from.getch() )
		{
			++extracted_spaces;
		}

		from.putback();

		result.first = true;
		if( extracted_spaces > 0u )
		{
			result.second = ' ';
		}

		return result;
	}
};

//
// token_t
//
class token_t
	:	public producer_tag<
			std::string,
			construction_time_t::compiletime >
{
	RESTINIO_NODISCARD
	static bool
	try_parse_value( source_t & from, std::string & accumulator )
	{
		do
		{
			const auto ch = from.getch();
			if( ch.m_eof )
				break;

			if( !is_token_char(ch.m_ch) )
			{
				from.putback();
				break;
			}

			accumulator += ch.m_ch;
		}
		while( true );

		if( accumulator.empty() )
			return false;

		return true;
	}

	RESTINIO_NODISCARD
	static constexpr bool
	is_token_char( const char ch ) noexcept
	{
		return is_vchar(ch) || is_digit(ch) ||
				ch == '!' ||
				ch == '#' ||
				ch == '$' ||
				ch == '%' ||
				ch == '&' ||
				ch == '\'' ||
				ch == '*' ||
				ch == '+' ||
				ch == '-' ||
				ch == '.' ||
				ch == '^' ||
				ch == '_' ||
				ch == '`' ||
				ch == '|' ||
				ch == '~';
	}

public :
	RESTINIO_NODISCARD
	auto
	try_parse( source_t & from ) const
	{
		std::pair< bool, std::string > result;
		result.first = try_parse_value( from, result.second );
		return result;
	}
};

//
// quoted_string_t
//
class quoted_string_t
	:	public producer_tag<
			std::string,
			construction_time_t::compiletime >
{
	RESTINIO_NODISCARD
	static bool
	try_parse_value( source_t & from, std::string & accumulator )
	{
		bool second_quote_extracted{ false };
		do
		{
			const auto ch = from.getch();
			if( ch.m_eof )
				break;

			if( '"' == ch.m_ch )
				second_quote_extracted = true;
			else if( '\\' == ch.m_ch )
			{
				const auto next = from.getch();
				if( next.m_eof )
					break;
				else if( SP == next.m_ch || HTAB == next.m_ch ||
						is_vchar( next.m_ch ) ||
						is_obs_text( next.m_ch ) )
				{
					accumulator += next.m_ch;
				}
				else
					break;
			}
			else if( is_qdtext( ch.m_ch ) )
				accumulator += ch.m_ch;
			else
				break;
		}
		while( !second_quote_extracted );

		return second_quote_extracted;
	}

public :
	RESTINIO_NODISCARD
	auto
	try_parse( source_t & from ) const
	{
		std::pair< bool, std::string > result;
		result.first = false;

		const auto ch = from.getch();
		if( !ch.m_eof )
		{
			if( '"' == ch.m_ch )
				result.first = try_parse_value( from, result.second );
			else
				from.putback();
		}

		return result;
	}
};

} /* namespace rfc */

//
// symbol_t
//
class symbol_t
	:	public producer_tag<
			char,
			construction_time_t::compiletime >
{
	char m_expected;

public:
	constexpr symbol_t( char expected ) : m_expected{ expected } {}

	RESTINIO_NODISCARD
	auto
	try_parse( source_t & from ) const noexcept
	{
		std::pair< bool, char > result;

		const auto ch = from.getch();
		if( !ch.m_eof && ch.m_ch == m_expected )
		{
			result.second = ch.m_ch;
			result.first = true;
		}
		else
		{
			from.putback();
			result.first = false;
		}

		return result;
	}
};

//
// any_value_skipper_t
//
struct any_value_skipper_t
{
	template< typename Target_Type, typename Value >
	void
	consume( Target_Type &, Value && ) const noexcept {}
};

//
// as_result_consumer_t
//
struct as_result_consumer_t
{
	template< typename Target_Type, typename Value >
	void
	consume( Target_Type & dest, Value && src ) const
		noexcept(noexcept(dest=std::forward<Value>(src)))
	{
		dest = std::forward<Value>(src);
	}
};

//
// custom_consumer_t
//
template< typename C >
class custom_consumer_t
{
	C m_consumer;

public :
	custom_consumer_t( C && consumer ) : m_consumer{std::move(consumer)} {}

	template< typename Target_Type, typename Value >
	void
	consume( Target_Type & dest, Value && src ) const
	{
		m_consumer( dest, std::forward<Value>(src) );
	}
};

//
// field_setter_t
//
template< typename F, typename C >
class field_setter_t
{
	using pointer_t = F C::*;

	pointer_t m_ptr;

public :
	field_setter_t( pointer_t ptr ) noexcept : m_ptr{ptr} {}

	void
	consume( C & to, F && value ) const
		noexcept(noexcept(to.*m_ptr = std::move(value)))
	{
		to.*m_ptr = std::move(value);
	}
};

template< typename P, typename F, typename C >
RESTINIO_NODISCARD
clause_t< value_producer_t<P>, value_consumer_t< field_setter_t<F,C> > >
operator>>( value_producer_t<P> producer, F C::*member_ptr )
{
	return {
			std::move(producer),
			value_consumer_t< field_setter_t<F,C> >{
					field_setter_t<F,C>{ member_ptr }
			}
	};
}

//
// to_lower_t
//
struct to_lower_t : public transformer_tag< std::string, std::string >
{
	RESTINIO_NODISCARD
	result_type
	transform( incoming_type && input ) const noexcept
	{
		result_type result{ std::move(input) };
		std::transform( result.begin(), result.end(), result.begin(),
			[]( unsigned char ch ) -> char {
				return static_cast<char>(
						restinio::impl::to_lower_lut<unsigned char>()[ch]
				);
			} );

		return result;
	}
};

} /* namespace impl */

//
// produce
//
template< typename Target_Type, typename... Clauses >
RESTINIO_NODISCARD
auto
produce( Clauses &&... clauses )
{
	using producer_type_t = impl::produce_t<
			Target_Type,
			std::tuple<Clauses...> >;

	using result_type_t = impl::value_producer_t< producer_type_t >;

	return result_type_t{
			producer_type_t{
					std::make_tuple(std::forward<Clauses>(clauses)...)
			}
	};
}

//
// alternatives
//
template< typename Target_Type, typename... Producers >
RESTINIO_NODISCARD
auto
alternatives( Producers &&... producers )
{
	using producer_type_t = impl::alternatives_t<
			Target_Type,
			std::tuple<Producers...> >;

	using result_type_t = impl::value_producer_t< producer_type_t >;

	return result_type_t{
			producer_type_t{
					std::make_tuple(std::forward<Producers>(producers)...)
			}
	};
}

//
// optional
//
template< typename Target_Type, typename... Clauses >
RESTINIO_NODISCARD
auto
optional( Clauses &&... clauses )
{
	using producer_type_t = impl::optional_producer_t<
			Target_Type,
			std::tuple<Clauses...> >;

	using result_type_t = impl::value_producer_t< producer_type_t >;

	return result_type_t{
			producer_type_t{
					std::make_tuple(std::forward<Clauses>(clauses)...)
			}
	};
}

//
// repeat
//
template<
	typename Container,
	template<class C> class Container_Adaptor = default_container_adaptor,
	typename... Clauses >
RESTINIO_NODISCARD
auto
repeat(
	std::size_t min_occurences,
	std::size_t max_occurences,
	Clauses &&... clauses )
{
	using producer_type_t = impl::repeat_t<
			Container_Adaptor<Container>,
			std::tuple<Clauses...> >;

	using result_type_t = impl::value_producer_t< producer_type_t >;

	return result_type_t{
			producer_type_t{
					min_occurences,
					max_occurences,
					std::make_tuple(std::forward<Clauses>(clauses)...)
			}
	};
}

//
// one_or_more_of_t
//
template<
	typename Container,
	template<class C> class Container_Adaptor = default_container_adaptor,
	typename Producer >
RESTINIO_NODISCARD
auto
one_or_more_of(
	impl::value_producer_t<Producer> producer )
{
	using producer_type_t = impl::one_or_more_of_t<
			Container_Adaptor<Container>,
			Producer >;

	using result_type_t = impl::value_producer_t< producer_type_t >;

	return result_type_t{
			producer_type_t{ producer.giveaway() }
	};
}

//
// symbol
//
RESTINIO_NODISCARD
impl::value_producer_t< impl::symbol_t >
symbol( char expected ) noexcept { return { impl::symbol_t{expected} }; }

//
// into
//
template< typename F, typename C >
RESTINIO_NODISCARD
impl::value_consumer_t< impl::field_setter_t<F, C> >
into( F C::*ptr ) noexcept
{
	return { impl::field_setter_t<F, C>{ptr} };
}

//
// skip
//
RESTINIO_NODISCARD
impl::value_consumer_t< impl::any_value_skipper_t >
skip() noexcept { return { impl::any_value_skipper_t{} }; }

//
// as_result
//
RESTINIO_NODISCARD
impl::value_consumer_t< impl::as_result_consumer_t >
as_result() noexcept { return { impl::as_result_consumer_t{} }; }

//
// custom_consumer
//
template< typename F >
RESTINIO_NODISCARD
auto
custom_consumer( F consumer )
{
	using actual_consumer_t = impl::custom_consumer_t< F >;

	return impl::value_consumer_t< actual_consumer_t >{
			actual_consumer_t{ std::move(consumer) }
	};
}

//
// to_lower
//
RESTINIO_NODISCARD
impl::value_transformer_t< impl::to_lower_t >
to_lower() noexcept { return { impl::to_lower_t{} }; }

namespace rfc
{

//
// ows
//
constexpr impl::value_producer_t< impl::rfc::ows_t > ows{
	impl::rfc::ows_t{}
};

//
// token
//
constexpr impl::value_producer_t< impl::rfc::token_t > token{
	impl::rfc::token_t{}
};

//
// quoted_string
//
constexpr impl::value_producer_t< impl::rfc::quoted_string_t >
quoted_string{
	impl::rfc::quoted_string_t{}
};

} /* namespace rfc */

namespace impl
{

template< typename Container_Adaptor, typename Producer >
RESTINIO_NODISCARD
std::pair<
	bool,
	typename one_or_more_of_t<Container_Adaptor, Producer>::container_type >
one_or_more_of_t<Container_Adaptor, Producer>::try_parse( source_t & from )
{
	namespace hfp = restinio::http_field_parser;

	std::pair< bool, container_type > result;
	result.first = false;

	auto opt_intro_clause = repeat< nothing_t >( 0, N,
		symbol(',') >> skip(),
		hfp::rfc::ows >> skip() ) >> skip();
	if( opt_intro_clause.try_process( from, result.second ) )
	{
		auto the_first_item_clause = m_producer >> custom_consumer(
				[]( auto & dest, value_type && item ) {
					Container_Adaptor::store( dest, std::move(item) );
				} );
		if( the_first_item_clause.try_process( from, result.second ) )
		{
			auto remaining_item_clause =
				produce< restinio::optional_t<value_type> >(
					hfp::rfc::ows >> skip(),
					symbol(',') >> skip(),
					optional< value_type >(
						hfp::rfc::ows >> skip(),
						m_producer >> as_result()
					) >> as_result()
				) >> custom_consumer(
					[]( auto & dest, restinio::optional_t<value_type> && item )
					{
						if( item )
							Container_Adaptor::store( dest, std::move(*item) );
					} );

			bool process_result{ true };
			do
			{
				const auto pos = from.current_position();
				process_result = remaining_item_clause.try_process(
						from, result.second );
				if( !process_result )
					from.backto( pos );
			}
			while( process_result );

			result.first = true;
		}
	}

	return result;
}

} /* namespace impl */

//
// try_parse_field_value
//
template< typename Final_Value, typename ...Clauses >
RESTINIO_NODISCARD
auto
try_parse_field_value(
	string_view_t from,
	Clauses && ...clauses )
{
	using clauses_tuple_t = std::tuple< Clauses... >;

	impl::source_t source{ from };

	auto result = impl::top_level_clause_t< Final_Value, clauses_tuple_t >{
			std::make_tuple( std::forward<Clauses>(clauses)... )
		}.try_process( source );

	if( !result.first ||
			!impl::ensure_no_remaining_content( source ) )
	{
		result.first = false;
	}

	return result;
}

} /* namespace http_field_parser */

} /* namespace restinio */

