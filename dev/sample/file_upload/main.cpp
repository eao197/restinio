 /*
	Simple example using sendfile facility.
*/

#include <iostream>
#include <random>

#include <restinio/all.hpp>
#include <restinio/transforms/zlib.hpp>

#include <clara.hpp>
#include <fmt/format.h>

//
// app_args_t
//

//
// app_args_t
//

struct app_args_t
{
	bool m_help{ false };
	std::string m_dest_folder{ "." };
	std::string m_address{ "localhost" };
	std::uint16_t m_port{ 8080 };
	std::size_t m_pool_size{ 1 };
	bool m_trace_server{ false };

	static app_args_t
	parse( int argc, const char * argv[] )
	{
		using namespace clara;

		app_args_t result;

		auto cli =
			Opt( result.m_dest_folder, "destination folder" )
					["-d"]["--dest-folder"]
					( fmt::format( "destination folder for uploaded files"
							" (default: {})", result.m_dest_folder ) )
			| Opt( result.m_address, "address" )
					["-a"]["--address"]
					( fmt::format( "address to listen (default: {})", result.m_address ) )
			| Opt( result.m_port, "port" )
					["-p"]["--port"]
					( fmt::format( "port to listen (default: {})", result.m_port ) )
			| Opt( result.m_pool_size, "thread-pool size" )
					[ "-n" ][ "--thread-pool-size" ]
					( fmt::format(
						"The size of a thread pool to run server (default: {})",
						result.m_pool_size ) )
			| Opt( result.m_trace_server )
					[ "-t" ][ "--trace" ]
					( "Enable trace server" )
			| Help(result.m_help);

		auto parse_result = cli.parse( Args(argc, argv) );
		if( !parse_result )
		{
			throw std::runtime_error{
				fmt::format(
					"Invalid command-line arguments: {}",
					parse_result.errorMessage() ) };
		}

		if( result.m_help )
		{
			std::cout << cli << std::endl;
		}

		return result;
	}
};

using router_t = restinio::router::express_router_t<>;

bool starts_with(
	const restinio::string_view_t & where,
	const restinio::string_view_t & what ) noexcept
{
	return where.size() >= what.size() &&
			0 == where.compare(0u, what.size(), what);
}

std::string to_string(
	const restinio::string_view_t & what )
{
	return { what.data(), what.size() };
}

restinio::string_view_t get_boundary(
	const restinio::request_handle_t & req )
{
	// There should be content-type field.
	const auto content_type = req->header().value_of(
			restinio::http_field::content_type );

	if( !starts_with( content_type, "multipart/form-data" ) )
		throw std::runtime_error( "unexpected value of content-type field: " +
				to_string( content_type ) );

	const restinio::string_view_t boundary_mark{ "boundary=" };
	const auto boundary_pos = content_type.find( boundary_mark );
	if( restinio::string_view_t::npos == boundary_pos )
		throw std::runtime_error( "there is no 'boundary' in content-type field: " +
				to_string( content_type ) );

	const auto boundary = content_type.substr(
			boundary_pos + boundary_mark.size() );
	if( boundary.empty() )
		throw std::runtime_error( "empty 'boundary' in content-type field: " +
				to_string( content_type ) );

	return boundary;
}

restinio::optional_t< restinio::string_view_t > opt_field_value(
	const restinio::string_view_t & line,
	const restinio::string_view_t & field_name )
{
	restinio::optional_t< restinio::string_view_t > result;

	const auto field_pos = line.find( field_name );
	if( restinio::string_view_t::npos != field_pos )
	{
		if( line.substr( field_pos + field_name.size(), 2u ) == "=\"" )
		{
			const auto value_pos = field_pos + field_name.size() + 2u;
			const auto end = line.find( '"', value_pos );
			if( restinio::string_view_t::npos != end )
				result = line.substr( value_pos, end - value_pos );
		}
	}

	return result;
}

struct line_from_buffer_t
{
	restinio::string_view_t m_line;
	restinio::string_view_t m_remaining_buffer;
};

line_from_buffer_t get_line_from_buffer(
	const restinio::string_view_t & buffer )
{
	const restinio::string_view_t eol{ "\r\n" };
	const auto pos = buffer.find( eol );
	if( restinio::string_view_t::npos == pos )
		throw std::runtime_error( "no lines with the correct EOL in the buffer" );

	return { buffer.substr( 0u, pos ), buffer.substr( pos + eol.size() ) };
}

bool try_handle_body_fragment(
	const app_args_t & args,
	restinio::string_view_t fragment )
{
	// Process fields at the beginning of the fragment.
	restinio::optional_t< restinio::string_view_t > file_name;
	for( auto line = get_line_from_buffer( fragment );
			!line.m_line.empty();
			line = get_line_from_buffer( line.m_remaining_buffer ) )
	{
		if( starts_with( line.m_line, "Content-Disposition: form-data;" ) )
		{
			const auto field_name = opt_field_value( line.m_line, "name" );
			if( field_name && *field_name == "file" )
			{
				file_name = opt_field_value( line.m_line, "filename" );
			}
		}
	}

	if( file_name )
	{
		std::cout << "# name of uploading file is: " << *file_name << std::endl;
	}

	return false;
}

void save_file(
	const app_args_t & args,
	const restinio::request_handle_t & req )
{
	const auto boundary = get_boundary( req );

	const restinio::string_view_t eol{ "\r\n" };

	std::cout << "*** boundary: " << boundary << std::endl;

	restinio::string_view_t full_body_view{ req->body() };
	restinio::string_view_t remaining_body_view{ full_body_view };
	while( remaining_body_view.size() > boundary.size() )
	{
		auto start = remaining_body_view.find( boundary );
		if( restinio::string_view_t::npos == start )
			break;
		else
			start += boundary.size();

		const auto end = remaining_body_view.find( boundary, start );
		if( restinio::string_view_t::npos == end )
			break;

		auto fragment = remaining_body_view.substr( start, end - start );
		if( starts_with( fragment, eol ) )
			fragment = fragment.substr( eol.size() );
		
		if( try_handle_body_fragment( args, fragment ) )
			break;

		remaining_body_view = remaining_body_view.substr( end );
	}

}

auto make_router( const app_args_t & args )
{
	auto router = std::make_unique< router_t >();

	router->http_get(
		"/",
		[ & ]( const restinio::request_handle_t& req, auto ){
			const auto action_url = fmt::format( "http://{}:{}/upload",
					args.m_address, args.m_port );

			auto resp = req->create_response();
			resp.append_header( "Server", "RESTinio" );
			resp.append_header_date_field();
			resp.append_header(
					restinio::http_field::content_type,
					"text/html; charset=utf-8" );
			resp.set_body(
R"---(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>File Upload!</title>
</head>
<body>
<p>Please select file to be uploaded to server.</p>
<form method="post" action=")---" + action_url + R"---(" enctype="multipart/form-data">
    <p><input type="text" name="comment" id="comment-id" value=""></p>
    <p><input type="file" name="file" id="file-id"></p>
    <p><button type="submit">Submit</button></p>
</form>
</body>
</html>
)---" );
			return resp.done();
		} );

	router->http_post( "/upload",
		[&]( const auto & req, const auto & )
		{
			save_file( args, req );

			auto resp = req->create_response();
			resp.append_header( "Server", "RESTinio" );
			resp.append_header_date_field();
			resp.append_header(
					restinio::http_field::content_type,
					"text/plain; charset=utf-8" );
			resp.set_body( "Ok. Uploaded" );
			return resp.done();
		} );

	return router;
}

template < typename Server_Traits >
void run_server( const app_args_t & args )
{
	restinio::run(
		restinio::on_thread_pool< Server_Traits >( args.m_pool_size )
			.port( args.m_port )
			.address( args.m_address )
			.concurrent_accepts_count( args.m_pool_size )
			.request_handler( make_router( args ) ) );
}

int main( int argc, const char * argv[] )
{
	try
	{
		const auto args = app_args_t::parse( argc, argv );

		if( !args.m_help )
		{
			if( args.m_trace_server )
			{
				using traits_t =
					restinio::traits_t<
						restinio::asio_timer_manager_t,
						restinio::shared_ostream_logger_t,
						router_t >;

				run_server< traits_t >( args );
			}
			else
			{
				using traits_t =
					restinio::traits_t<
						restinio::asio_timer_manager_t,
						restinio::null_logger_t,
						router_t >;

				run_server< traits_t >( args );
			}
		}
	}
	catch( const std::exception & ex )
	{
		std::cerr << "Error: " << ex.what() << std::endl;
		return 1;
	}

	return 0;
}

