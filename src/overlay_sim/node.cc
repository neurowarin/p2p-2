#include "node.h"

node::node(
	const int & ID_in
):
	ID(ID_in)
{

}

int node::get_ID()
{
	return ID;
}
