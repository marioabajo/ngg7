/* 
 * This file is part of the XXX distribution (https://github.com/xxxx or http://xxx.github.io).
 * Copyright (c) 2017 Mario Abajo
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

/*
comments about the lzma files inside firmware: 
   to compres:   ./lzma -z -7 FILE
   to decompres: ./lzma -d FILE
   use old lzma utils: https://tukaani.org/lzma/
   because they put the expanded file size in the header
*/

// the NGG7 file header
struct __attribute__((__packed__)) file_header
{
// 32bytes header LZMA after that + "00" byte at the end of file...

	char head[4];    // NGG7
	char ver[4];     // 2.00
	uint16_t code1;  // 0x1e2f or 0x382e or 0x212f
	uint16_t const1; // 0x0f00
	int const2;      // 0x0405de07 or 0x040507de
	int32_t code2;   // file size - header?
};

// the header of a file that has a lazma file inside
struct __attribute__((__packed__)) lzma_header
{
	uint64_t size_d; // file size decompresed
	int zero1;       // 0
};

// the header of a packet file
struct __attribute__((__packed__)) pack_header
{
	uint32_t code1;  // unknown
	char info[8];
	uint32_t num;
	uint32_t code2;  // unknown
	uint32_t code3;  // unknown
	uint32_t code4;  // unknown
};

// a section (file info) inside a packed file
struct __attribute__((__packed__)) section_header
{       
        char section_name[16];
        int offset;
        int size;
        int code1;  // unknown
        int code2;  // unknown
};

int copy_content(int org, int dst, off_t offset, int size)
{
        char buff[2048];
        off_t pos;
        int copied = 0, aux;

	// save actual position in the file
        pos = lseek(org, 0, SEEK_CUR);

	// go to the beginning of the content
        lseek(org, offset, SEEK_SET);

	// copy the given size into the new file
        while (copied < size)
        {
                if ((size - copied) < 2048)
                        aux = read(org, buff, size - copied);
                else
                        aux = read(org, buff, 2048);
                write(dst, buff, aux);
                copied += aux;
        }

	// go back to the last file position
        lseek(org, pos, SEEK_SET);
}

int pack_file(int fd, uint32_t size)
{
	int i, newf;
	struct pack_header ph;
	struct section_header sh;

	// Read the packet header
	read(fd, &ph, sizeof(ph));
        printf("Header: %.8s , sections: %d\n", ph.info, ph.num);

	// Iterate for every file that is inside
        for (i=0; i< ph.num; i++)
        {
		// Read the hedaer of this file section
                read(fd, &sh, sizeof(sh));
                printf("  Section: %s , pos: %d , size: %d\n", sh.section_name, sh.offset, sh.size);

		// Create the new file
		if ((newf = open(sh.section_name, O_RDWR | O_CREAT, S_IWUSR | \
				S_IRUSR | S_IWGRP | S_IRGRP | S_IWOTH | S_IROTH)) <= 0 )
		{
			perror("Error creating file");
			return -1;
		}
		copy_content(fd, newf, sh.offset, sh.size);
		close(newf);
        }
	return 0;
}

int lzma_file(int fd, uint32_t code2, char *filename)
{
	const int buffersize= 2048;
	struct lzma_header lh;
	char buff[buffersize];
	char extended_filename[256];
	int newf, aux;
	uint32_t copied = 0;

	// read the info header for a lzma file (not the lzma header)
	read(fd, &lh, sizeof(lh));
	printf("  Descompresed size: %d\n", lh.size_d);

	// compose the new filename and create it
	snprintf(extended_filename, 256, "%s.lzma", filename);
        if ((newf = open(extended_filename, O_RDWR | O_CREAT, S_IWUSR | \
                         S_IRUSR | S_IWGRP | S_IRGRP | S_IWOTH | S_IROTH)) <= 0 )
        {               
        	perror("Error creating file");
                return -1;
        }       

	// Copy content to new file
	do
	{
        	aux = read(fd, buff, buffersize);
		// do this to avoid writing the file terminating character (0)
		if (aux != buffersize)
			aux--;

		write(newf, buff, aux);
		copied += aux;
	}
	while (aux == buffersize);

	close(newf);
	printf ("  Size: %u\n", copied);

	// delete original file (don't care if its already open)
	unlink(filename);

	return 0;
}

int main (int argc, char *argv[])
{
	int i, fd;
	struct file_header fh;
	char *p;

	// open file
	if ((fd = open(argv[1], O_RDONLY)) <= 0)
	{
		perror("Error opening file");
		return -1;
	}

	// read first header
	read(fd, &fh, sizeof(fh));
	printf("Header: %.4s v.%.4s , code1: %hd code2: %u const1: %hd const2: %u\n", fh.head, fh.ver, fh.code1, fh.code2, fh.const1, fh.const2);

	// if it's a Pack file
	if (fh.code2 != 65536 && fh.code2 != 1048576)
		pack_file(fd, fh.code2);
	// if it's a lzma file
	else
		lzma_file(fd, fh.code2, argv[1]);

	// close
	close(fd);
}
