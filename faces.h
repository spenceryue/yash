// working ascii emojis

#ifndef FACES_H
#define FACES_H


#define pikachu "\n\
 █▀▀▄           ▄▀▀█\n\
 █░░░▀▄ ▄▄▄▄▄ ▄▀░░░█\n\
  ▀▄░░░▀░░░░░▀░░░▄▀\n\
   ▐░░▄▀░░░▀▄░░▌▄▄▀▀▀▀█\n\
   ▌▄▄▀▀░▄░▀▀▄▄▐░░░░░░█\n\
▄▀▀▐▀▀░▄▄▄▄▄░▀▀▌▄▄▄░░░█\n\
█░░░▀▄░█░░░█░▄▀░░░░█▀▀▀\n\
 ▀▄░░▀░░▀▀▀░░▀░░░▄█▀\n\
   █░░░░░░░░░░░▄▀▄░▀▄\n\
   █░░░░░░░░░▄▀█  █░░█\n\
   █░░░░░░░░░░░█▄█░░▄▀\n\
   █░░░░░░░░░░░████▀\n\
   ▀▄▄▀▀▄▄▀▀▄▄▄█▀"


#define stare			"( ͡° ͜ʖ ͡°)"
#define flip_table		"(╯°□°)╯.-~ ┻━┻"
#define blank_face		"(●__●)"
#define check_mark		"✔"
#define x_mark			"✖"
#define porter_robinson	"【=◈︿◈=】"
#define fart_right		"\\(´Д` )/==3"
#define fart_left		"ε≡≡\\( ´Д`)/"

#endif /* FACES_H */



/* Test Faces */
#if __INCLUDE_LEVEL__ == 0 && defined __INCLUDE_LEVEL__
	#include <iostream>
	#include <string>

using namespace std;

int main(int argc, char* argv[])
{
	cout << pikachu << endl << endl;
	cout << stare << endl << endl;
	cout << flip_table << endl << endl;
	cout << blank_face << endl << endl;
	cout << check_mark << endl << endl;
	cout << porter_robinson << endl << endl;
	cout << fart_left << endl << endl;
	cout << fart_right << endl << endl;

  return 0;
}
#endif
/* Test Faces */