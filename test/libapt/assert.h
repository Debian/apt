#include <iostream>

#define equals(x,y) assertEquals(y, x, __LINE__)

template < typename X, typename Y >
void OutputAssertEqual(X expect, char const* compare, Y get, unsigned long const &line) {
	std::cerr << "Test FAILED: »" << expect << "« " << compare << " »" << get << "« at line " << line << std::endl;
}

template < typename X, typename Y >
void assertEquals(X expect, Y get, unsigned long const &line) {
	if (expect == get)
		return;
	OutputAssertEqual(expect, "==", get, line);
}

void assertEquals(unsigned int const &expect, int const &get, unsigned long const &line) {
	if (get < 0)
		OutputAssertEqual(expect, "==", get, line);
	assertEquals<unsigned int const&, unsigned int const&>(expect, get, line);
}

void assertEquals(int const &expect, unsigned int const &get, unsigned long const &line) {
	if (expect < 0)
		OutputAssertEqual(expect, "==", get, line);
	assertEquals<unsigned int const&, unsigned int const&>(expect, get, line);
}

void assertEquals(unsigned long const &expect, int const &get, unsigned long const &line) {
	if (get < 0)
		OutputAssertEqual(expect, "==", get, line);
	assertEquals<unsigned long const&, unsigned long const&>(expect, get, line);
}

void assertEquals(int const &expect, unsigned long const &get, unsigned long const &line) {
	if (expect < 0)
		OutputAssertEqual(expect, "==", get, line);
	assertEquals<unsigned long const&, unsigned long const&>(expect, get, line);
}


#define equalsOr2(x,y,z) assertEqualsOr2(y, z, x, __LINE__)

template < typename X, typename Y >
void OutputAssertEqualOr2(X expect1, X expect2, char const* compare, Y get, unsigned long const &line) {
	std::cerr << "Test FAILED: »" << expect1 << "« or »" << expect2 << "« " << compare << " »" << get << "« at line " << line << std::endl;
}

template < typename X, typename Y >
void assertEqualsOr2(X expect1, X expect2, Y get, unsigned long const &line) {
	if (expect1 == get || expect2 == get)
		return;
	OutputAssertEqualOr2(expect1, expect2, "==", get, line);
}

void assertEqualsOr2(unsigned int const &expect1, unsigned int const &expect2, int const &get, unsigned long const &line) {
	if (get < 0)
		OutputAssertEqualOr2(expect1, expect2, "==", get, line);
	assertEqualsOr2<unsigned int const&, unsigned int const&>(expect1, expect2, get, line);
}

void assertEqualsOr2(int const &expect1, int const &expect2, unsigned int const &get, unsigned long const &line) {
	if (expect1 < 0 && expect2 < 0)
		OutputAssertEqualOr2(expect1, expect2, "==", get, line);
	assertEqualsOr2<unsigned int const&, unsigned int const&>(expect1, expect2, get, line);
}


#define equalsOr3(w,x,y,z) assertEqualsOr3(x, y, z, w, __LINE__)

template < typename X, typename Y >
void OutputAssertEqualOr3(X expect1, X expect2, X expect3, char const* compare, Y get, unsigned long const &line) {
	std::cerr << "Test FAILED: »" << expect1 << "« or »" << expect2 << "« or »" << expect3 << "« " << compare << " »" << get << "« at line " << line << std::endl;
}

template < typename X, typename Y >
void assertEqualsOr3(X expect1, X expect2, X expect3, Y get, unsigned long const &line) {
	if (expect1 == get || expect2 == get || expect3 == get)
		return;
	OutputAssertEqualOr3(expect1, expect2, expect3, "==", get, line);
}


// simple helper to quickly output a vectors
template < typename X >
void dumpVector(X vec) {
	for (typename X::const_iterator v = vec.begin();
	     v != vec.end(); ++v)
		std::cout << *v << std::endl;
}
