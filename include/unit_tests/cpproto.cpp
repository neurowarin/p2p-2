//include
#include <cpproto/cpproto.hpp>
#include <unit_test.hpp>
#include <logger.hpp>

int fail(0);

//one of every ASCII character
const std::string test_str(
	" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNO"
	"PQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~"
);

CPPROTO_MESSAGE_BEGIN(nested_message, 0)
	CPPROTO_FIELD(cpproto::ASCII<0>, ASCII)
	CPPROTO_FIELD(cpproto::boolean<1>, boolean)
	CPPROTO_FIELD(cpproto::string<2>, string)
	CPPROTO_FIELD(cpproto::sint<3>, sint)
	CPPROTO_FIELD(cpproto::uint<4>, uint)
	CPPROTO_FIELD(cpproto::list<cpproto::ASCII<5> >, ASCII_list)
CPPROTO_MESSAGE_END

CPPROTO_MESSAGE_BEGIN(message, 1)
	CPPROTO_FIELD(cpproto::ASCII<1>, ASCII)
	CPPROTO_FIELD(cpproto::boolean<2>, boolean)
	CPPROTO_FIELD(cpproto::string<3>, string)
	CPPROTO_FIELD(cpproto::sint<4>, sint)
	CPPROTO_FIELD(cpproto::uint<5>, uint)
	CPPROTO_FIELD(cpproto::list<cpproto::ASCII<6> >, ASCII_list)
	CPPROTO_FIELD(cpproto::list<nested_message>, Nested_Message_list)
CPPROTO_MESSAGE_END

//set test values on nested_message
void nested_message_set_test_vals(nested_message & NM)
{
	NM.ASCII = test_str;
	NM.boolean = false;
	NM.string = test_str;
	NM.uint = 123;
	NM.sint = -123;
	NM.ASCII_list->push_back(test_str);
	NM.ASCII_list->push_back(test_str);
}

//set test values on message
void message_set_test_vals(message & M)
{
	M.ASCII = test_str;
	M.boolean = true;
	M.string = test_str;
	M.uint = 123;
	M.sint = -123;
	M.ASCII_list->push_back(test_str);
	M.ASCII_list->push_back(test_str);
	nested_message tmp;
	nested_message_set_test_vals(tmp);
	M.Nested_Message_list->push_back(tmp);
	tmp.uint = 321;
	M.Nested_Message_list->push_back(tmp);
}

void ASCII_field()
{
	cpproto::ASCII<0> field = test_str, parsed_field;
	if(!parsed_field.parse(field.serialize())){
		LOG; ++fail;
	}
	if(!field || !parsed_field || *field != *parsed_field){
		LOG; ++fail;
	}
}

void boolean_field()
{
	cpproto::boolean<0> field = true, parsed_field;
	if(!parsed_field.parse(field.serialize())){
		LOG; ++fail;
	}
	if(!field || !parsed_field || *field != *parsed_field){
		LOG; ++fail;
	}
	field = false;
	if(!parsed_field.parse(field.serialize())){
		LOG; ++fail;
	}
	if(!field || !parsed_field || *field != *parsed_field){
		LOG; ++fail;
	}
}

void uint_field()
{
	cpproto::uint<0> field = 123, parsed_field;
	if(!parsed_field.parse(field.serialize())){
		LOG; ++fail;
	}
	if(!field || !parsed_field || *field != *parsed_field){
		LOG; ++fail;
	}
}

void sint_field()
{
	cpproto::sint<0> field = -123, parsed_field;
	if(!parsed_field.parse(field.serialize())){
		LOG; ++fail;
	}
	if(!field || !parsed_field || *field != *parsed_field){
		LOG; ++fail;
	}
}

void string_field()
{
	cpproto::string<0> field = test_str, parsed_field;
	if(!parsed_field.parse(field.serialize())){
		LOG; ++fail;
	}
	if(!field || !parsed_field || *field != *parsed_field){
		LOG; ++fail;
	}
}

void list_field()
{
	{//BEGIN scope
	//list of non-length_delimited values
	cpproto::list<cpproto::uint<0> > field, parsed_field;
	field->push_back(0);
	field->push_back(1);
	field->push_back(2);
	if(!parsed_field.parse(field.serialize())){
		LOG; ++fail;
	}
	if(*field != *parsed_field){
		LOG; ++fail;
	}
	}//END scope

	{//BEGIN scope
	//list of length_delimited values
	cpproto::list<cpproto::string<0> > field, parsed_field;
	field->push_back("ABCD");
	field->push_back("123");
	field->push_back("VWXYZ");
	if(!parsed_field.parse(field.serialize())){
		LOG; ++fail;
	}
	if(*field != *parsed_field){
		LOG; ++fail;
	}
	}//END scope

	{//BEGIN scope
	//list of lists
	cpproto::list<cpproto::list<cpproto::uint<0> > > field, parsed_field;
	cpproto::list<cpproto::uint<0> > field_element;
	field_element->push_back(0);
	field_element->push_back(1);
	field_element->push_back(2);
	field->push_back(field_element);
	field->push_back(field_element);
	field->push_back(field_element);
	if(!parsed_field.parse(field.serialize())){
		LOG; ++fail;
	}
	if(*field != *parsed_field){
		LOG; ++fail;
	}
	}//END scope
}

void message_field()
{
	message field, parsed_field;
	message_set_test_vals(field);
	if(!parsed_field.parse(field.serialize())){
		LOG; ++fail;
	}
	if(field != parsed_field){
		LOG; ++fail;
	}
}

void unknown_field()
{
	//old message version
	class m_old : public cpproto::message<0>
	{
	public:
		m_old()
		{
			add_field(string);
		}
		cpproto::string<1> string;
	} M_Old;

	//new message version that has additional field
	class m_new : public cpproto::message<0>
	{
	public:
		m_new()
		{
			add_field(string);
			add_field(uint);

			string = test_str;
			uint = 123;
		}

		cpproto::string<1> string;
		cpproto::uint<2> uint;
	} M_New;
	if(!M_Old.parse(M_New.serialize())){
		LOG; ++fail;
	}
	//M_Old holds unknown field
	if(M_Old != M_New){
		LOG; ++fail;
	}
	//equivalent to M_Old != M_New
	if(M_Old.serialize() != M_New.serialize()){
		LOG; ++fail;
	}
}

void parser()
{
	cpproto::parser Parser;
	Parser.add_field<message>();
	message field;
	message_set_test_vals(field);
	std::string buf;
	buf += field.serialize();
	buf += field.serialize();
	unsigned test_parsed = 0;
	while(boost::shared_ptr<cpproto::field> M = Parser.parse(buf)){
		if(typeid(*M) == typeid(message)){
			message * parsed_field = (message *)M.get();
			if(field != *parsed_field){
				LOG; ++fail;
			}
			++test_parsed;
		}
	}
	if(test_parsed != 2){
		LOG; ++fail;
	}
	if(Parser.bad_stream()){
		LOG; ++fail;
	}
}

int main()
{
	unit_test::timeout();

	ASCII_field();
	boolean_field();
	uint_field();
	sint_field();
	list_field();
	message_field();
	unknown_field();

	parser();

	return fail;
}
