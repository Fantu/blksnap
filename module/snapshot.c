#include "common.h"
#ifdef MODSECTION
#undef MODSECTION
#define MODSECTION "-snapshot"
#endif

#include "snapshot.h"
#include "tracker.h"
#include "snapimage.h"
#include "tracking.h"

LIST_HEAD(snapshots);
DECLARE_RWSEM(snapshots_lock);

void _snapshot_destroy( snapshot_t* p_snapshot );

void snapshot_Done( void )
{
	snapshot_t* snap;
	pr_info( "Removing all snapshots\n" );

	do {
		snap = NULL;
		down_write( &snapshots_lock );
		if (!list_empty( &snapshots )) {
			snapshot_t* snap = list_entry( snapshots.next, snapshot_t, link );

			list_del( &snap->link );
		}
		up_write( &snapshots_lock );

		if (snap)
			_snapshot_destroy( snap );

	} while(snap);
}

int _snapshot_New( dev_t* p_dev, int count, snapshot_t** pp_snapshot )
{
	snapshot_t* p_snapshot = NULL;
	dev_t* snap_set = NULL;

	p_snapshot = (snapshot_t*)kzalloc( sizeof(snapshot_t), GFP_KERNEL );
	if (NULL == p_snapshot){
		pr_err( "Unable to create snapshot: failed to allocate memory for snapshot structure\n" );
		return -ENOMEM;
	}
	INIT_LIST_HEAD( &p_snapshot->link );

	p_snapshot->id = (unsigned long)(p_snapshot );

	snap_set = (dev_t*)kzalloc( sizeof( dev_t ) * count, GFP_KERNEL );
	if (NULL == snap_set) {
		kfree(p_snapshot);

		pr_err( "Unable to create snapshot: faile to allocate memory for snapshot map\n" );
		return -ENOMEM;
	}
	memcpy( snap_set, p_dev, sizeof( dev_t ) * count );

	p_snapshot->dev_id_set_size = count;
	p_snapshot->dev_id_set = snap_set;

	down_write(&snapshots_lock);
	list_add_tail(&snapshots, &p_snapshot->link);
	up_write(&snapshots_lock);

	*pp_snapshot = p_snapshot;

	return SUCCESS;
}

int _snapshot_remove_device( dev_t dev_id )
{
	int result;
	tracker_t* tracker = NULL;

	result = tracker_find_by_dev_id( dev_id, &tracker );
	if (result != SUCCESS){
		if (result == -ENODEV)
			pr_err( "Cannot find device by device id=[%d:%d]\n", 
				MAJOR(dev_id), MINOR(dev_id) );
		else
			pr_err( "Failed to find device by device id=[%d:%d]\n", 
				MAJOR(dev_id), MINOR(dev_id) );
		return SUCCESS;
	}

	if (result != SUCCESS)
		return result;

	tracker_snapshot_id_set(tracker, 0ull);

	pr_info( "Device [%d:%d] successfully removed from snapshot\n",
		MAJOR( dev_id ), MINOR( dev_id ) );
	return SUCCESS;
}

void _snapshot_cleanup( snapshot_t* snapshot )
{
	int inx = 0;

	for (; inx < snapshot->dev_id_set_size; ++inx){
		int result = _snapshot_remove_device( snapshot->dev_id_set[inx] );
		if (result != SUCCESS)
			pr_err( "Failed to remove device [%d:%d] from snapshot\n",
				MAJOR(snapshot->dev_id_set[inx]), MINOR(snapshot->dev_id_set[inx]) );
	}

	if (snapshot->dev_id_set != NULL)
		kfree( snapshot->dev_id_set );
	kfree( snapshot );
}

int snapshot_Create( dev_t* dev_id_set, unsigned int dev_id_set_size, unsigned int cbt_block_size_degree, unsigned long long* psnapshot_id )
{
	snapshot_t* snapshot = NULL;
	int result = SUCCESS;
	unsigned int inx = 0;

	pr_info( "Create snapshot for devices:\n" );
	for (inx = 0; inx < dev_id_set_size; ++inx){
		dev_t dev_id = dev_id_set[inx];
		pr_info( "\t%d:%d\n", MAJOR( dev_id ), MINOR( dev_id ) );
	}
	result = _snapshot_New( dev_id_set, dev_id_set_size, &snapshot );
	if (result != SUCCESS){
		pr_err( "Unable to create snapshot: failed to allocate snapshot structure\n" );
		return result;
	}
	do{
		result = -ENODEV;
		for (inx = 0; inx < snapshot->dev_id_set_size; ++inx)
		{
			dev_t dev_id = snapshot->dev_id_set[inx];

			result = tracking_add(dev_id, cbt_block_size_degree, snapshot->id);
			if (result == -EALREADY)
				result = SUCCESS;
			else if (result != SUCCESS){
				pr_err( "Unable to create snapshot: failed to add device [%d:%d] to snapshot tracking\n",
					MAJOR( dev_id ), MINOR( dev_id ) );
				break;
			}
		}
		if (result != SUCCESS)
			break;


		result = tracker_capture_snapshot( snapshot );
		if (SUCCESS != result){
			pr_err( "Unable to create snapshot: failed to capture snapshot [0x%llx]\n", snapshot->id );
			break;
		}

		result = snapimage_create_for( snapshot->dev_id_set, snapshot->dev_id_set_size );
		if (result != SUCCESS){
			pr_err( "Unable to create snapshot: failed to create snapshot image devices\n" );

			tracker_release_snapshot( snapshot );
			break;
		}

		*psnapshot_id = snapshot->id;
		pr_info( "Snapshot [0x%llx] was created\n", snapshot->id );
	} while (false);

	if (SUCCESS != result) {
		pr_info( "Snapshot [0x%llx] cleanup\n", snapshot->id );

		down_write( &snapshots_lock );
		list_del( &snapshot->link );
		up_write( &snapshots_lock );

		_snapshot_cleanup( snapshot );
	}
	return result;
}

void _snapshot_destroy( snapshot_t* snapshot )
{
	int result = SUCCESS;
	size_t inx;

	for (inx = 0; inx < snapshot->dev_id_set_size; ++inx){
		/*int res = */snapimage_stop( snapshot->dev_id_set[inx] );
	}

	result = tracker_release_snapshot( snapshot );
	if (result != SUCCESS){
		pr_err( "Failed to release snapshot [0x%llx]\n", snapshot->id );
	}

	for (inx = 0; inx < snapshot->dev_id_set_size; ++inx){
		/*int res = */snapimage_destroy( snapshot->dev_id_set[inx] );
	}

	_snapshot_cleanup( snapshot );
}

int snapshot_Destroy( unsigned long long snapshot_id )
{
	snapshot_t* snapshot = NULL;

	pr_info( "Destroy snapshot [0x%llx]\n", snapshot_id );

	down_read( &snapshots_lock );
	if ( !list_empty(&snapshots) ) {
		struct list_head* _head;

		list_for_each( _head, &snapshots ){
			snapshot_t *_snap = list_entry(_head, snapshot_t, link);

			if (_snap->id == snapshot_id){
				snapshot = _snap;
				list_del( &snapshot->link );
				break;
			}
		}
	}
	up_read( &snapshots_lock );

	if (NULL == snapshot) {
		pr_err( "Unable to destroy snapshot [0x%llx]: cannot find snapshot by id\n", snapshot_id );
		return -ENODEV;
	}
	
	_snapshot_destroy(snapshot);
	return SUCCESS;
}
