#ifndef _CONTAINERS_H
#define _CONTAINERS_H

void container_add_actuator (struct pn_actuator *container, struct pn_actuator *a);
void container_remove_actuator (struct pn_actuator *container, struct pn_actuator *a);
void container_unlink_actuators (struct pn_actuator *container);

#endif /* _CONTAINERS_H */
