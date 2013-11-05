#ifndef _COMPUTER_H_
#define _COMPUTER_H_

#include "output.h"
#include "json.h"
#include <vector>
#include <string>

#define DEFAULT_COMPUTER_NAME ""

class game;
class player;

enum computer_action
{
    COMPACT_NULL = 0,
    COMPACT_OPEN,
    COMPACT_LOCK,
    COMPACT_UNLOCK,
    COMPACT_TOLL,
    COMPACT_SAMPLE,
    COMPACT_RELEASE,
    COMPACT_TERMINATE,
    COMPACT_PORTAL,
    COMPACT_CASCADE,
    COMPACT_RESEARCH,
    COMPACT_MAPS,
    COMPACT_MAP_SEWER,
    COMPACT_MISS_LAUNCH,
    COMPACT_MISS_DISARM,
    COMPACT_LIST_BIONICS,
    COMPACT_ELEVATOR_ON,
    COMPACT_AMIGARA_LOG,
    COMPACT_AMIGARA_START,
    COMPACT_DOWNLOAD_SOFTWARE,
    COMPACT_BLOOD_ANAL,
    COMPACT_DATA_ANAL,
    COMPACT_DISCONNECT,
    COMPACT_STEMCELL_TREATMENT,
    COMPACT_EMERG_MESS,
    COMPACT_TOWER_UNRESPONSIVE,
    COMPACT_SR1_MESS, //Security Reminders for Hazardous Waste Sarcophagus (SRCF)
    COMPACT_SR2_MESS,
    COMPACT_SR3_MESS,
    COMPACT_SR4_MESS,
    COMPACT_SRCF_1_MESS,
    COMPACT_SRCF_2_MESS,
    COMPACT_SRCF_3_MESS,
    COMPACT_SRCF_SEAL_ORDER,
    COMPACT_SRCF_SEAL,
    COMPACT_SRCF_ELEVATOR,
    NUM_COMPUTER_ACTIONS
};

enum computer_failure
{
    COMPFAIL_NULL = 0,
    COMPFAIL_SHUTDOWN,
    COMPFAIL_ALARM,
    COMPFAIL_MANHACKS,
    COMPFAIL_SECUBOTS,
    COMPFAIL_DAMAGE,
    COMPFAIL_PUMP_EXPLODE,
    COMPFAIL_PUMP_LEAK,
    COMPFAIL_AMIGARA,
    COMPFAIL_DESTROY_BLOOD,
    COMPFAIL_DESTROY_DATA,
    NUM_COMPUTER_FAILURES
};

struct computer_option
{
    std::string name;
    computer_action action;
    int security;

    computer_option()
    {
        name = "Unknown", action = COMPACT_NULL, security = 0;
    };
    computer_option(std::string N, computer_action A, int S) :
        name (N), action (A), security (S) {};
};

class compsec
{
protected:
    compsec(){}
public:
    virtual ~compsec(){}
    virtual bool attempt() = 0;
};

class compsec_pass : public compsec
{
public:
    compsec_pass(std::string password) : pass(password){}
    ~compsec_pass(){}
    bool attempt();
    std::string pass;
};

class compsec_hack : public compsec
{
public:
    compsec_hack(int difficulty) : diff(difficulty){}
    ~compsec_hack(){}
    bool attempt();
    int diff;
};

class compsec_item : public compsec
{
public:
    compsec_item(std::string item, int quantity) : it(item), num(quantity){}
    ~compsec_item(){}
    bool attempt();
    std::string it;
    int num;
};

class compsec_itemat
{
public:
    compsec_itemat(std::string item, int x, int y) : it(item), itemx(x), itemy(y){}
    ~compsec_itemat(){}
    bool attempt();
    std::string it;
    int itemx;
    int itemy;
};

class compact
{
protected:
    compact(){}
public:
    virtual ~compact(){}
    virtual void go() = 0;
};

// computer action used to change terrain (open doors)
class compact_chter : public compact
{
public:
    compact_chter(int x, int y, std::string terrain) : terx(x), tery(y), ter(terrain){}
    ~compact_chter(){}
    void go();
    int terx;
    int tery;
    std::string ter;
};

// computer action to display message
class compact_msg : public compact
{
public:
    compact_msg(std::string message) : msg(message) {}
    ~compact_msg(){}
    void go();
    std::string msg;
};

// computer action to change level (elevator)
class compact_chlvl : public compact
{
public:
    compact_chlvl(int lvls) : z(lvls) {}
    ~compact_chlvl(){}
    void go();
    int z;
};

class compact_noise : public compact
{
public:
    compact_noise(int volume, std::string description) : vol(volume), desc(description) {}
    ~compact_noise(){}
    void go();
    int vol;
    std::string desc;
};

class compact_mon : public compact
{
public:
    compact_mon(int x, int y, std::string monster) : monx(x), mony(y), mon(monster) {}
    ~compact_mon(){}
    void go();
    int monx;
    int mony;
    std::string mon;
};

class compact_item : public compact
{
public:
    compact_item(int x, int y, std::string item) : itemx(x), itemy(y), it(item) {}
    ~compact_item(){}
    void go();
    int itemx;
    int itemy;
    std::string it;
};

class compact_map : public compact
{
public:
    compact_map(int radius, int zlvl, std::vector<int> omtypes = std::vector<int>()) : rad(radius), z(zlvl), types(omtypes){}
    ~compact_map(){}
    void go();
    int rad;
    int z;
    std::vector<int> types;
};

class compopt
{
public:
    compopt(std::string msg) : prompt(msg) {}
    ~compopt(){}
    void go();
    void add_security(compsec*);
    void add_action(compact*);
    void add_failure(compact*);

    std::vector<compsec*> security;
    std::vector<compact*> actions;
    std::vector<compact*> failures;
    std::string prompt;
};

class computer
{
public:
    computer();
    computer(std::string Name, int Security);
    ~computer();

    computer & operator=(const computer &rhs);

    void add_compopt(compopt option);
    void use(bool test);

    // Initialization
    void set_security(int Security);
    void add_option(std::string opt_name, computer_action action, int Security);
    void add_failure(computer_failure failure);
    // Basic usage
    void shutdown_terminal(); // Shutdown (free w_terminal, etc)
    void use(game *g);
    bool hack_attempt(game *g, player *p, int Security = -1);// -1 defaults to main security
    // Save/load
    std::string save_data();
    void load_data(std::string data);

    std::string name; // "Jon's Computer", "Lab 6E77-B Terminal Omega"
    int mission_id; // Linked to a mission?

    static void load_lab_note(JsonObject &jsobj);

private:

    std::vector<compopt> compopts;

    // Difficulty of simply logging in
    int security;
    // Things we can do
    std::vector<computer_option> options;
    // Things that happen if we fail a hack
    std::vector<computer_failure> failures;
    // Output window
    WINDOW *w_terminal;
    // Pretty border
    WINDOW *w_border;
    // Misc research notes from json
    static std::vector<std::string> lab_notes;

    // Called by use()
    void activate_function      (game *g, computer_action action);
    // Generally called when we fail a hack attempt
    void activate_random_failure(game *g);
    // ...but we can also choose a specific failure.
    void activate_failure       (game *g, computer_failure fail);

    // OUTPUT/INPUT:

    // Reset to a blank terminal (e.g. at start of usage loop)
    void reset_terminal();
    // Prints a line to the terminal (with printf flags)
    void print_line(const char *text, ...);
    // For now, the same as print_line but in red (TODO: change this?)
    void print_error(const char *text, ...);
    // Wraps and prints a block of text with a 1-space indent
    void print_text(const char* text, ...);
    // Prints code-looking gibberish
    void print_gibberish_line();
    // Prints a line and waits for Y/N/Q
    char query_ynq(const char *text, ...);
    // Same as query_ynq, but returns true for y or Y
    bool query_bool(const char *text, ...);
    // Simply wait for any key, returns True
    bool query_any(const char *text, ...);
    // Move the cursor to the beginning of the next line
    void print_newline();
};

#endif
