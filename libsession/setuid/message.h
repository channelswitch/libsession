struct message_in;
int message_in_deserialize(struct message_in **m_p, char **buf, int *buf_sz);
void message_in_free(struct message_in *m);
