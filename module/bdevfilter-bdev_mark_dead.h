/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (C) 2023 Veeam Software Group GmbH */

static int bdevfilter_set(struct ftrace_ops *ops, unsigned char *name);
static void bdevfilter_unset(struct ftrace_ops *ops);

static unsigned long addr_bdev_mark_dead;

/*
 * ftrace for bdev_mark_dead()
 */
static notrace __attribute__((optimize("no-optimize-sibling-calls")))
void bdev_mark_dead_handler(struct block_device *bdev, bool surprise)
{
	pr_debug("Mark device '%d:%d' dead\n",
		MAJOR(bdev->bd_dev), MINOR(bdev->bd_dev));
	__blkfilter_detach(bdev->bd_dev, NULL, 0);
	/*
	 * bdev_mark_dead(bdev, surprise);
	 * On some systems, this function may not be exported.
	 */
	((void (*)(struct block_device *bdev, bool surprise))addr_bdev_mark_dead)(bdev, surprise);
}

static notrace void ftrace_handler_bdev_mark_dead(
	unsigned long ip, unsigned long parent_ip, struct ftrace_ops *fops,
#ifdef HAVE_FTRACE_REGS
	struct ftrace_regs *fregs
#else
	struct pt_regs *regs
#endif
	)
{
	if (within_module(parent_ip, THIS_MODULE))
		return;

#if defined(HAVE_FTRACE_REGS_SET_INSTRUCTION_POINTER)
	ftrace_regs_set_instruction_pointer(fregs, (unsigned long)bdev_mark_dead_handler);
#elif defined(HAVE_FTRACE_REGS)
	ftrace_instruction_pointer_set(fregs, (unsigned long)bdev_mark_dead_handler);
#else
	instruction_pointer_set(regs, (unsigned long)bdev_mark_dead_handler);
#endif
}

static struct ftrace_ops ops_bdev_mark_dead = {
	.func = ftrace_handler_bdev_mark_dead,
	.flags = FTRACE_OPS_FL_DYNAMIC |
		FTRACE_OPS_FL_SAVE_REGS |
		FTRACE_OPS_FL_IPMODIFY |
		FTRACE_OPS_FL_PERMANENT,
};

static inline int prepare_functions(unsigned long kernel_base)
{
	int ret;
	void *addr;

	ret = get_symbol("bdev_mark_dead", &addr);
	if (ret)
		pr_err("Failed to get address of the '%s'\n", "bdev_mark_dead");
	else {
		addr_bdev_mark_dead = kernel_base + (unsigned long)addr;
		pr_debug("Function '%s' has been found\n", "bdev_mark_dead");
	}
	return ret;
}

static inline int set_functions(void)
{
	return bdevfilter_set(&ops_bdev_mark_dead, "bdev_mark_dead");
}

static inline void unset_functions(void)
{
	bdevfilter_unset(&ops_bdev_mark_dead);
}
