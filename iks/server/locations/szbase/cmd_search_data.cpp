#include <boost/algorithm/string.hpp>

#include "../../../config.h"
#include "data/szbase_wrapper.h"
#include "utils/tokens.h"
#include "szbase.h"

#include "cmd_search_data.h"

SearchDataRcv::SearchDataRcv( Vars& vars , SzbaseProt& prot )
	: vars(vars)
	, prot(prot)
{
	set_next( std::bind(&SearchDataRcv::parse_command,this,std::placeholders::_1) );
}

void SearchDataRcv::parse_command( const std::string& data )
{
	namespace balgo = boost::algorithm;

	auto r = find_quotation( data );
	std::string name(r.begin(),r.end());

	auto l = find_after_quotation( data ) ;
	std::vector<std::string> tags;
	balgo::split(
			tags ,
			l ,
			balgo::is_any_of(" "), balgo::token_compress_on );

	if( tags.size() != 5 ) {
		fail( ErrorCodes::ill_formed );
		return;
	}

	ProbeType pt(tags[0]);

	SearchDir dir;
	if( tags[1] == "left" ) {
		dir = SearchDir::LEFT;
	} else if( tags[1] == "right" ) {
		dir = SearchDir::RIGHT;
	} else {
		fail( ErrorCodes::ill_formed );
		return;
	}

	TimeType ttype;
	if( tags[2] == "second_t" ) {
		ttype = TimeType::SECOND;
	} else if( tags[2] == "nanosecond_t" ) {
		ttype = TimeType::NANOSECOND;
	} else {
		fail( ErrorCodes::ill_formed );
		return;
	}
		
	try {
		apply( vars.get_szbase()->search_data( prot.get_mapped_param_name( name )
						     , tags[3] , tags[4], ttype , dir , pt ) );
	} catch ( szbase_param_not_found_error& ) {
		fail( ErrorCodes::unknown_param );
	} catch ( szbase_error& e ) {
		fail( ErrorCodes::szbase_error , e.what() );
	}
	
}


