#include <iostream>

#define equals(x,y) assertEquals(x, y, __LINE__)

template < typename X, typename Y >
void OutputAssert(X expect, char const* compare, Y get, unsigned long const &line) {
	std::cerr << "Test FAILED: »" << expect << "« " << compare << " »" << get << "« at line " << line << std::endl;
}

template < typename X, typename Y >
void assertEquals(X expect, Y get, unsigned long const &line) {
	if (expect == get)
		return;
	OutputAssert(expect, "==", get, line);
}

void assertEquals(unsigned int const &expect, int const &get, unsigned long const &line) {
	if (get < 0)
		OutputAssert(expect, "==", get, line);
	assertEquals<unsigned int const&, unsigned int const&>(expect, get, line);
}
