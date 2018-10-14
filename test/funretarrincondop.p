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