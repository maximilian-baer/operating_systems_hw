#define SPINLOCK_H

int tas(volatile char *lock);

void spin_lock(volatile char *lock);
void spin_unlock(volatile char *lock);
