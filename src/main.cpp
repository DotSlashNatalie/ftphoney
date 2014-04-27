#include <ptypes/ptypes.h>
#include <ptypes/pasync.h>
#include <ptypes/pinet.h>
#include <ptypes/ptime.h>
#include <cstdlib>

USING_PTYPES;

semaphore s(10);

#define DEF_KEEP_ALIVE_TIMEOUT  15000

void writeLog(ipaddress ip, string resp, string ask)
{
	const char * fmt = "%m-%d-%Y_%H:%M:%S";
	string out = "[" + dttostring(now(), fmt) + " - " + iptostring(ip) +  "] - " + ask + " => " + resp;
	pout.putline(out);
}

void writeClient(ipstream * client, string msg)
{
	client->putline(msg);
	client->flush();
	writeLog(client->get_ip(), msg, "SERVER");
}

class ftp : public thread
{
	protected:
	virtual void execute();
	virtual void cleanup() { delete stream; }
private:
	ipstream * stream;
	void commands();
public:
	ftp(ipstream * client) : thread(true), stream(client) { }
};

void ftp::commands()
{
	while(true)
	{
		if (!stream->get_active())
            break;
        if (!stream->waitfor(DEF_KEEP_ALIVE_TIMEOUT))
            break;
        if (stream->get_eof())
            break;
		string s;
		s = stream->line(256);
		writeLog(stream->get_ip(), s, "REQUEST");
		if (lowercase(s) == "syst")
		{
			writeClient(stream, "215 UNIX Type: L8");
		} else if ( lowercase(s) == "feat" )
		{
			writeClient(stream, "211-Features:");
			writeClient(stream, "EPRT");
			writeClient(stream, "EPSV");
			writeClient(stream, "MDTM");
			writeClient(stream, "PASV");
			writeClient(stream, "REST STREAM");
			writeClient(stream, "TVFS");
			writeClient(stream, "UTF8");
			writeClient(stream, "211 END");
		} else if ( pos("TYPE", s) != -1 )
		{
			writeClient(stream, "200 Switching modes");
		} else if ( lowercase(s) == "pwd")
		{
			writeClient(stream, "257 \"/\""); // root directory access oh yeah!
		} else if ( pos("OPTS", s) != -1 ) 
		{ 
			writeClient(stream, "200 OK"); // hehe - accept setting any options
		} else if ( lowercase(s) == "quit")
		{
			writeClient(stream, "215 goodbye");
			break;
		} else {
			writeClient(stream, "200 OK"); // whatever you say client!
		}
	}
}

void ftp::execute()
{
	try
	{
		string s;
		if (stream->get_active())
		{
			writeClient(stream, "220 ProFTPD 1.3.2c Server (datanethost.net_FTP_Service)");
			// client enters username
			s = stream->line(256);
			writeLog(stream->get_ip(), s, "BANNER");
			
			writeClient(stream, "330 Password required to access user account");
			
			s = stream->line(256);
			writeLog(stream->get_ip(), s, "PASSWORD");
			
			writeClient(stream, "230 Logged in.");
			commands();

		}
	} catch (estream * e)
	{
		pout.putline(e->get_message());
		perr.putline("ERROR!");
		delete e;
	} catch (...)
	{
		perr.putline("ERROR2!");
	}
	stream->close();
	s.signal();
}

class ftpthread : public thread, protected ipstmserver
{
	protected:
		virtual void execute();     // override thread::execute()
		virtual void cleanup() { }     // override thread::cleanup()
private:
	int port;
	tobjlist<ftp> ftpworkers;
	public:
		ftpthread(int port) : thread(false), port(port) {}
		virtual ~ftpthread() { waitfor(); }
};

void ftpthread::execute()
{
	
	bindall(port);
	while(true)
	{
		ipstream * client = new ipstream();
		s.wait();
		serve(*client);
		ftp * f = new ftp(client); // thread will automatically be freed
								   // don't believe me?
								   // I don't either. check pthread.cxx
								   // also Dr. Memory verifies this
								   // I'll believe Dr. Memory over you
		f->start();
	}
}

int main(int argc, char * args[])
{
	int port = atoi(args[1]);
	ftpthread thftp(port);
	thftp.start();

	thftp.waitfor();
	return 0;
}