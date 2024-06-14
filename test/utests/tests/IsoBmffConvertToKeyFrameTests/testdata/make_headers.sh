#!/bin/bash

rm -f testdata.h
testdata=()

cat >> testdata.h <<'headermsg'
// DO NOT EDIT
// This is an automatically generated file using make_headers.sh

// To recreate it or add new test data rerun make_headers.sh.
// New testdata should be in an .m4s files had have the following name formats:
//      source:     meaningfullFileName.m4d
//      expected:   meaningfullFileName_expected.m4d


headermsg

for f in *expected.m4s; do
	xxd -i ${f%_expected.m4s}.m4s >> testdata.h
	echo $'' >> testdata.h
	xxd -i $f >> testdata.h
	testdata+=("    {\"${f%_expected.m4s}\", ${f%_expected.m4s}_m4s, ${f%_expected.m4s}_m4s_len, ${f%.m4s}_m4s, ${f%.m4s}_m4s_len},")
	echo $'\n' >> testdata.h
done

cat >> testdata.h <<'endstruct'

struct test_data_t
{
	const std::string	test_name;
	unsigned char * 	input_data;
	uint32_t 			input_data_len;
	unsigned char * 	expected_data;
	uint32_t 			expected_data_len;
} test_data[] = {
endstruct
for ((i = 0; i < ${#testdata[@]}; i++)); do
	echo '    '${testdata[$i]} >> testdata.h
done
echo '};' >> testdata.h
