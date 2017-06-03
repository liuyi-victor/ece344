#include "testfs.h"
#include "list.h"
#include "super.h"
#include "block.h"
#include "inode.h"

/* given logical block number, read the corresponding physical block into block.
 * return physical block number.
 * returns 0 if physical block does not exist.
 * returns negative value on other errors. */
static int
testfs_read_block(struct inode *in, int log_block_nr, char *block)
{
	int phy_block_nr = 0;

	assert(log_block_nr >= 0);
	if (log_block_nr >= (NR_DIRECT_BLOCKS + NR_INDIRECT_BLOCKS + NR_INDIRECT_BLOCKS*NR_INDIRECT_BLOCKS))
	{
		if(EFBIG > 0)
			return -EFBIG;
		return EFBIG;
	}
	if (log_block_nr < NR_DIRECT_BLOCKS)
	{
		phy_block_nr = (int)in->in.i_block_nr[log_block_nr];
	}
	else
	{
		log_block_nr -= NR_DIRECT_BLOCKS;		//# of extra blocks beyond the direct blocks

		if (log_block_nr >= NR_INDIRECT_BLOCKS)		//in double indirect blocks range
		{
			//TBD();
			if (in->in.i_dindirect > 0)
			{
				read_blocks(in->sb, block, in->in.i_dindirect, 1);		//read the double indirect block
				log_block_nr -= NR_INDIRECT_BLOCKS;				//# of extra blocks beyond both the direct blocks and indirect blocks
			
				int indirectnum = log_block_nr / NR_INDIRECT_BLOCKS;		//which indirect block is it
				int indirectoffset = log_block_nr % NR_INDIRECT_BLOCKS;		//which direct block it is within the indirect block
				int single_indirect_nr = ((int *)block)[indirectnum];		//get the block # of the indirect block
				if(single_indirect_nr > 0)
				{
				    read_blocks(in->sb, block, single_indirect_nr, 1);		//read the direct block # from the obtained indirect block
				    phy_block_nr = ((int *)block)[indirectoffset];
				}
			}
		}
		//if (in->in.i_indirect > 0) {
		else
		{
		    if (in->in.i_indirect > 0)
		    {
			read_blocks(in->sb, block, in->in.i_indirect, 1);
			phy_block_nr = ((int *)block)[log_block_nr];
		    }
		}
	}
	//done finding the "physical block number". use this number to read actual disk blocks
	if (phy_block_nr > 0) {
		read_blocks(in->sb, block, phy_block_nr, 1);
	} else {
		/* we support sparse files by zeroing out a block that is not
		 * allocated on disk. */
		bzero(block, BLOCK_SIZE);
	}
	return phy_block_nr;
}

int
testfs_read_data(struct inode *in, char *buf, off_t start, size_t size)
{
	char block[BLOCK_SIZE];
	long block_nr = start / BLOCK_SIZE;             //logical block numbers used to access the file
	long block_ix = start % BLOCK_SIZE;
	long index = 0;
	int ret;

	assert(buf);
	if (start + (off_t) size > in->in.i_size) {	//read range beyond the file. Therefore read until the eof
		size = in->in.i_size - start;
	}
	size_t remaining = size;
	
	if ((ret = testfs_read_block(in, block_nr, block)) < 0)
		return ret;
	if(block_ix + size <= BLOCK_SIZE)
	{
		memcpy(buf, block + block_ix, size);
		remaining = 0;
	}
	else
	{
		memcpy(buf, block + block_ix, BLOCK_SIZE - block_ix);
		remaining = remaining - (BLOCK_SIZE - block_ix);
	}
	index = BLOCK_SIZE - block_ix;
	block_nr = block_nr + 1;
	//if (block_ix + size > BLOCK_SIZE) 
	//if(remaining > 0)//the requested data spans across multiple file blocks
	//{
	//TBD();
	while(remaining > 0)
	{
		if ((ret = testfs_read_block(in, block_nr, block)) < 0)
			return ret;
		//memcpy(buf, block, size);
		if(remaining >= BLOCK_SIZE)
		{
			memcpy(buf+index, block, BLOCK_SIZE);
			remaining = remaining - BLOCK_SIZE;
			index += BLOCK_SIZE;
		}
		else
		{
			memcpy(buf+index, block, remaining);
			remaining = remaining - remaining;
			//assert((index + BLOCK_SIZE) >= size);
		}	
		block_nr = block_nr + 1;
	}
	//}
	/* return the number of bytes read or any error */
	return size;
}

/* given logical block number, allocate a new physical block, if it does not
 * exist already, and return the physical block number that is allocated.
 * returns negative value on error. */
static int
testfs_allocate_block(struct inode *in, int log_block_nr, char *block)
{
	int phy_block_nr;
	char indirect[BLOCK_SIZE];
	int indirect_allocated = 0;
	int double_indirect_allocated = 0;

	assert(log_block_nr >= 0);
	phy_block_nr = testfs_read_block(in, log_block_nr, block);

	/* phy_block_nr > 0: block exists, so we don't need to allocate it, 
	   phy_block_nr < 0: some error */
	if (phy_block_nr != 0)
		return phy_block_nr;

	/* allocate a direct block */
	if (log_block_nr < NR_DIRECT_BLOCKS)
	{
		assert(in->in.i_block_nr[log_block_nr] == 0);
		phy_block_nr = testfs_alloc_block_for_inode(in);
		if (phy_block_nr >= 0)
		{
			in->in.i_block_nr[log_block_nr] = phy_block_nr;
			//testfs_sync_inode(in);
		}
		return phy_block_nr;
	}
	//do not forget to do inode block sync on disk filesystem (testfs_sync_inode(struct inode *in))
	log_block_nr -= NR_DIRECT_BLOCKS;
	if (log_block_nr >= NR_INDIRECT_BLOCKS)        //in the double indirect blocks range
	{
		//TBD();
		char doubleindirect[BLOCK_SIZE];
		log_block_nr -= NR_INDIRECT_BLOCKS;
		if(in->in.i_dindirect == 0)
		{
			//allocate a double indirect block
			bzero(doubleindirect, BLOCK_SIZE);
			phy_block_nr = testfs_alloc_block_for_inode(in);
			if (phy_block_nr < 0)
				return phy_block_nr;
			double_indirect_allocated = 1;
			in->in.i_dindirect = phy_block_nr;
			//did not do the inode sync will wait for later
		}
		else
		{
			//simply read the double indirect block
			read_blocks(in->sb, doubleindirect, in->in.i_dindirect, 1);
		}
			//done allocating the double indirect block now allocating the indirect block
		  //assert(((int *)indirect)[log_block_nr] == 0);

		  int indirectnum = log_block_nr / NR_INDIRECT_BLOCKS;				//which indirect block is it
		  int indirectoffset = log_block_nr % NR_INDIRECT_BLOCKS;			//which direct block it is within the indirect block
		  //int single_indirect_nr = ((int *)doubleindirect)[indirectnum];		//get the block # of the indirect block
		  
		  if (((int *)doubleindirect)[indirectnum] == 0)
		  {	
			//need to allocate this indirect block
			  bzero(indirect, BLOCK_SIZE);
			  phy_block_nr = testfs_alloc_block_for_inode(in);

			  if (phy_block_nr >= 0) 
			  {
				// allocate success, update double indirect block 
				((int *)doubleindirect)[indirectnum] = phy_block_nr;
				write_blocks(in->sb, doubleindirect, in->in.i_dindirect, 1);
				indirect_allocated = 1;
			  } 
			  else 
			  {
				if (double_indirect_allocated) 
				{
					// free the double indirect block that was allocated
					testfs_free_block_from_inode(in, in->in.i_dindirect);
					in->in.i_dindirect = 0;
				}
				return phy_block_nr;
			  }
		  }
		  else
		  {
			//simply read the indirect block
			read_blocks(in->sb, indirect, ((int *)doubleindirect)[indirectnum], 1);
		  }
		  // allocate direct block
		  assert(((int *)indirect)[indirectoffset] == 0);	
		  phy_block_nr = testfs_alloc_block_for_inode(in);

		  if (phy_block_nr >= 0) 
		  {
			// update indirect block
			((int *)indirect)[indirectoffset] = phy_block_nr;
			write_blocks(in->sb, indirect, ((int *)doubleindirect)[indirectnum], 1);
			//testfs_sync_inode(in);
		  } 
		  else  
		  {
			// free the indirect block that was allocated
			if(indirect_allocated)
			{
				// free the indirect block that was allocated
				testfs_free_block_from_inode(in, ((int *)doubleindirect)[indirectnum]);
				((int *)doubleindirect)[indirectnum] = 0;
				write_blocks(in->sb, doubleindirect, in->in.i_dindirect, 1);
			}
			if(double_indirect_allocated)
			{
				testfs_free_block_from_inode(in, in->in.i_dindirect);
				in->in.i_dindirect = 0;
			}
		  }
		  return phy_block_nr;
	}
	else
	{
		if (in->in.i_indirect == 0)
		{	/* allocate an indirect block */
			bzero(indirect, BLOCK_SIZE);
			phy_block_nr = testfs_alloc_block_for_inode(in);
			if (phy_block_nr < 0)
				return phy_block_nr;
			indirect_allocated = 1;
			in->in.i_indirect = phy_block_nr;
			
		}
		else
		{	/* read indirect block */
			read_blocks(in->sb, indirect, in->in.i_indirect, 1);
		}

		/* allocate direct block */
		assert(((int *)indirect)[log_block_nr] == 0);	
		phy_block_nr = testfs_alloc_block_for_inode(in);

		if (phy_block_nr >= 0) 
		{
			/* update indirect block */
			((int *)indirect)[log_block_nr] = phy_block_nr;
			write_blocks(in->sb, indirect, in->in.i_indirect, 1);
			//testfs_sync_inode(in);
		} 
		else if (indirect_allocated) 
		{
			/* free the indirect block that was allocated */
			testfs_free_block_from_inode(in, in->in.i_indirect);
			in->in.i_indirect = 0;
		}
		return phy_block_nr;
	}
}

int
testfs_write_data(struct inode *in, const char *buf, off_t start, size_t size)
{
	char block[BLOCK_SIZE];
	long block_nr = start / BLOCK_SIZE;
	long block_ix = start % BLOCK_SIZE;
	long index = 0;
	int ret;

	//if (block_ix + size > BLOCK_SIZE){
		//TBD();
	size_t remaining = size;

	ret = testfs_allocate_block(in, block_nr, block);
	if (ret < 0)
		return ret;
	if(block_ix + size <= BLOCK_SIZE)
	{
		memcpy(block + block_ix, buf, size);
		remaining = 0;
		in->in.i_size = MAX(in->in.i_size, start + (off_t) size);
	}
	else
	{
		memcpy(block + block_ix, buf, BLOCK_SIZE - block_ix);
		remaining = remaining - (BLOCK_SIZE - block_ix);
		in->in.i_size = MAX(in->in.i_size, start + (off_t) (BLOCK_SIZE - block_ix));
	}
	write_blocks(in->sb, block, ret, 1);
	index = BLOCK_SIZE - block_ix;
	block_nr += 1;
	//if (block_ix + size > BLOCK_SIZE) 
	//if(remaining > 0)//the requested data spans across multiple file blocks
	//{
	//TBD();
	in->i_flags |= I_FLAGS_DIRTY;
	while(remaining > 0)
	{
		if ((ret = testfs_allocate_block(in, block_nr, block)) < 0)
			return ret;

		if(remaining >= BLOCK_SIZE)
		{
			memcpy(block, buf+index, BLOCK_SIZE);
			remaining = remaining - BLOCK_SIZE;
			index += BLOCK_SIZE;
			in->in.i_size = MAX(in->in.i_size, block_nr*BLOCK_SIZE + BLOCK_SIZE);
		}
		else
		{
			memcpy(block, buf+index, remaining);
			in->in.i_size = MAX(in->in.i_size, block_nr*BLOCK_SIZE + (off_t)remaining);
			remaining = remaining - remaining;
			//assert((index + BLOCK_SIZE) >= size);
		}	
		write_blocks(in->sb, block, ret, 1);
		block_nr += 1;
	}
	//}
	//}
	/* ret is the newly allocated physical block number */
	//ret = testfs_allocate_block(in, block_nr, block);
	//if (ret < 0)
	//	return ret;
	//memcpy(block + block_ix, buf, size);
	//write_blocks(in->sb, block, ret, 1);
	/* increment i_size by the number of bytes written. */
	//if (size > 0)
	//	in->in.i_size = MAX(in->in.i_size, start + (off_t) size);
	//in->i_flags |= I_FLAGS_DIRTY;
	/* return the number of bytes written or any error */
	return size;
}

int
testfs_free_blocks(struct inode *in)
{
	int i;
	int e_block_nr;

	/* last block number */
	e_block_nr = DIVROUNDUP(in->in.i_size, BLOCK_SIZE);

	/* remove direct blocks */
	for (i = 0; i < e_block_nr && i < NR_DIRECT_BLOCKS; i++) {
		if (in->in.i_block_nr[i] == 0)
			continue;
		testfs_free_block_from_inode(in, in->in.i_block_nr[i]);
		in->in.i_block_nr[i] = 0;
	}
	e_block_nr -= NR_DIRECT_BLOCKS;

	/* remove indirect blocks */
	if (in->in.i_indirect > 0) 
	{
		char block[BLOCK_SIZE];
		read_blocks(in->sb, block, in->in.i_indirect, 1);
		for (i = 0; i < e_block_nr && i < NR_INDIRECT_BLOCKS; i++) 
		{
			testfs_free_block_from_inode(in, ((int *)block)[i]);
			((int *)block)[i] = 0;
		}
		testfs_free_block_from_inode(in, in->in.i_indirect);
		in->in.i_indirect = 0;
	}

	e_block_nr -= NR_INDIRECT_BLOCKS;
	if (e_block_nr >= 0) 
	{
		//TBD();
		if(in->in.i_dindirect > 0)
		{
			char block[BLOCK_SIZE];
			char doubleblock[BLOCK_SIZE];
			int j;
			read_blocks(in->sb, doubleblock, in->in.i_dindirect, 1);
			for (i = 0; i < NR_INDIRECT_BLOCKS; i++) 
			{
				if(((int *)doubleblock)[i] == 0)
					continue;
				read_blocks(in->sb, block, ((int *)doubleblock)[i], 1);
				//block is the array of pointers to data blocks
				for(j = 0; j < NR_INDIRECT_BLOCKS; j++) 
				{
					testfs_free_block_from_inode(in, ((int *)block)[j]);
					((int *)block)[j] = 0;
				}
				//e_block_nr -= NR_INDIRECT_BLOCKS;
				//free the indirect blocks pointed to by the double indirect block
				testfs_free_block_from_inode(in, ((int *)doubleblock)[i]);
				((int *)doubleblock)[i] = 0;
			}
			testfs_free_block_from_inode(in, in->in.i_dindirect);
			in->in.i_dindirect = 0;
		}
	}
	
	in->in.i_size = 0;
	in->i_flags |= I_FLAGS_DIRTY;
	return 0;
}
