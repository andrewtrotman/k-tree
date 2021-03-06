/*
	K_TREE_EXAMPLE.CPP
	------------------
	Copyright (c) 2020 Andrew Trotman
	Released under the 2-clause BSD license (See:https://en.wikipedia.org/wiki/BSD_licenses)
*/
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <fstream>
#include <iostream>

#include "k_tree.h"

/*
	READ_ENTIRE_FILE()
	------------------
	This uses a combination of "C" FILE I/O and C++ strings in order to copy the contents of a file into an internal buffer.
	There are many different ways to do this, but this is the fastest according to this link: http://insanecoding.blogspot.co.nz/2011/11/how-to-read-in-file-in-c.html
	Note that there does not appear to be a way in C++ to avoid the initialisation of the string buffer.

	Returns the length of the file in bytes - which is also the size of the string buffer once read.
*/
size_t read_entire_file(const std::string &filename, std::string &into)
	{
	FILE *fp;
	// "C" pointer to the file
#ifdef _MSC_VER
	struct __stat64 details;				// file system's details of the file
#else
	struct stat details;				// file system's details of the file
#endif
	size_t file_length = 0;			// length of the file in bytes

	/*
		Fopen() the file then fstat() it.  The alternative is to stat() then fopen() - but that is wrong because the file might change between the two calls.
	*/
	if ((fp = fopen(filename.c_str(), "rb")) != nullptr)
		{
#ifdef _MSC_VER
		if (_fstat64(fileno(fp), &details) == 0)
#else
		if (fstat(fileno(fp), &details) == 0)
#endif
			if ((file_length = details.st_size) != 0)
				{
				into.resize(file_length);
				if (fread(&into[0], details.st_size, 1, fp) != 1)
					into.resize(0);				// LCOV_EXCL_LINE	// happens when reading the file_size buyes failes (i.e. disk or file failure).
				}
		fclose(fp);
		}

	return file_length;
	}

/*
	BUFFER_TO_LIST()
	----------------
	Turn a single std::string into a vector of uint8_t * (i.e. "C" Strings). Note that these pointers are in-place.  That is,
	they point into buffer so any change to the uint8_t or to buffer effect each other.

	Note: This method removes blank lines from the input file.
*/
void buffer_to_list(std::vector<uint8_t *> &line_list, std::string &buffer)
	{
	uint8_t *pos;
	size_t line_count = 0;

	/*
		Walk the buffer counting how many lines we think are in there.
	*/
	pos = (uint8_t *)&buffer[0];
	while (*pos != '\0')
		{
		if (*pos == '\n' || *pos == '\r')
			{
			/*
				a seperate line is a consequative set of '\n' or '\r' lines.  That is, it removes blank lines from the input file.
			*/
			while (*pos == '\n' || *pos == '\r')
				pos++;
			line_count++;
			}
		else
			pos++;
		}

	/*
		resize the vector to the right size, but first clear it.
	*/
	line_list.clear();
	line_list.reserve(line_count);

	/*
		Now rewalk the buffer turning it into a vector of lines
	*/
	pos = (uint8_t *)&buffer[0];
	if (*pos != '\n' && *pos != '\r' && *pos != '\0')
		line_list.push_back(pos);
	while (*pos != '\0')
		{
		if (*pos == '\n' || *pos == '\r')
			{
			*pos++ = '\0';
			/*
				a seperate line is a consequative set of '\n' or '\r' lines.  That is, it removes blank lines from the input file.
			*/
			while (*pos == '\n' || *pos == '\r')
				pos++;
			if (*pos != '\0')
				line_list.push_back(pos);
			}
		else
			pos++;
		}
	}

/*
	BUILD()
	-------
	Build the k-tree from the input data
*/
int build(char *infilename, size_t tree_order, char *outfilename)
	{
	k_tree::allocator memory;
	std::vector<k_tree::object *> vector_list;

	/*
		Check the tree order is "reasonable"
	*/
	if (tree_order < 2 || tree_order > 1'000'000)
		exit(printf("Tree order must be between 2 and 1,000,000\n"));

	/*
		Read the source file into memory - and check that we got a file
	*/
	std::string file_contents;
	size_t bytes = read_entire_file(infilename, file_contents);
	if (bytes <= 0)
		exit(printf("Cannot read vector file: '%s'\n", infilename));

	/*
		Break it into lines
	*/
	std::vector<uint8_t *> lines;
	buffer_to_list(lines, file_contents);

	/*
		Count the dimensionality of the first vector (the others should be the same)
	*/
	size_t dimensions = 0;
	char *pos = (char *)lines[0];
	do
		{
		while (isspace(*pos))
			pos++;
		if (*pos == '\0')
			break;
		dimensions++;
		while (*pos != '\0' && !isspace(*pos))
			pos++;
		}
	while (*pos != '\0');

	/*
		Declare the tree
	*/
	k_tree::k_tree tree(&memory, tree_order, dimensions);
	k_tree::object *example_vector = tree.get_example_object();

	/*
		Convert each line into a vector and add it to a list (so that we can add them later)
	*/
	for (const auto line : lines)
		{
		size_t dimension = 0;
		k_tree::object *objectionable = example_vector->new_object(&memory);
		pos = (char *)line;

		do
			{
			while (isspace(*pos))
				pos++;
			if (*pos == '\0')
				break;
			float value = (float)atof(pos);
			while (*pos != '\0' && !isspace(*pos))
				pos++;
			objectionable->vector[dimension] = value;
			dimension++;
			}
		while (*pos != '\0');

		vector_list.push_back(objectionable);
		}

	/*
		Add them to the tree
	*/
	for (const auto vector : vector_list)
		tree.push_back(&memory, vector);

	/*
		Dump the tree to the output file
	*/
	std::ofstream outfile(outfilename);
	outfile << tree;
	outfile.close();

	return 0;
	}

/*
	UNITTEST()
	----------
	Unit test each of the classes we use
*/
int unittest(void)
	{
	k_tree::object::unittest();
	k_tree::k_tree::unittest();

	return 0;
	}

/*
	USAGE()
	-------
	Shlomo's original program took:
		Usage: ktree  ['build'|'load']  in_file  tree_order  out_file
	Where
		build | load is to build the k-tree or load one from disk (currently IGNORED)
		in_file is the ascii file of vectors, one pre line and human readable in ASCII
		tree_order is the branching factor of the k-tree
		out_file (IGNORED)
*/
int usage(char *exename)
	{
	std::cout << "Usage:" << exename << " <[build | unittest]> <in_file> <tree_order> <outfile>\n";
	return 0;
	}

/*
	MAIN()
	------
*/
int main(int argc, char *argv[])
	{
	if (argc != 5)
		if (argc == 2)
			return unittest();
		else
			return usage(argv[0]);
	else if (strcmp(argv[1], "unittest") == 0)
		return unittest();
	else if (strcmp(argv[1], "build") == 0)
		return build(argv[2], atoi(argv[3]), argv[4]);
	else
		return usage(argv[0]);
	}
