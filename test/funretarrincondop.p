native bool:p();

#ifdef BEFOREDEF
main() {
	printf(p() ? f() : "");
}
#endif

f() {
	new arr[256];
	return arr;
}

#ifdef NESTED
main() {
	printf(p() ? (p() ? f() : "") : "");
	printf(p() ? (p() ? "" : f()) : "");
	printf(p() ? (p() ? "" : "") : f());
}
#endif