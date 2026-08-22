/**
 * @brief Ripper - first-party RazionOS browser entry point.
 *
 * Ripper currently uses the native Razion Browser shell. Keeping this small
 * launcher separate gives the product a stable first-party command name
 * while the future sandboxed rendering engine remains replaceable.
 */
#include <stdio.h>
#include <unistd.h>

int main(int argc, char * argv[]) {
	argv[0] = "/bin/razion-browser";
	execv(argv[0], argv);
	perror("ripper");
	return 127;
}
