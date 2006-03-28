import java.io.File;
import java.util.GregorianCalendar;

import org.openafs.jafs.*;



/**
 * @version 	1.0
 * @author
 */
public class testAFS
{
	public class TesterThread implements Runnable
	{
		private String user = null;
		private String pass = null;
		private String cell = null;
		private boolean letItRun = true;

		public TesterThread(String user, String pass, String cell)
		{
			this.user = user;
			this.pass = pass;
			this.cell = cell;
		}
		public void finish()
		{
			letItRun = false;
		}
		public void run()
		{
			while(letItRun)
			{
				try
				{
					Token t = new Token(user, pass, cell);
					Cell c = new Cell(t, false);
					c.getInfo();
	
					for(int j=0; j<100; j++)
					{
						ACL acl = new ACL("/afs/." + cell, true);
					}

					c.close();
				} catch(Exception e) {
					e.printStackTrace();
				}
				Thread.yield();
			}
		}
	}
	
	
	static java.io.PrintStream out = System.out;
	static String firstCellPathRW = null;

	static boolean allowDump = true;
	
	static int ident = 0;

	static void DumpBegin()
	{
		if (allowDump)
		{
			for(int i=0; i<ident; i++)
				out.print("   ");
		}
	}
	static void Dump(String str)
	{
		if (allowDump)
			out.print(str);
	}
	static void DumpEnd()
	{
		if (allowDump)
			out.println();
	}
	static void DumpLn(String str)
	{
		DumpBegin();
		Dump(str);
		DumpEnd();
	}

	public static String getGregDateStr(GregorianCalendar gc)
	{
		if (gc==null)
			return "null";
		else
			return gc.getTime().toString();
	}

	public static void DumpToken(Token t)
	{
		try
		{
			DumpLn("Token: user=" + t.getUsername() +
					" cell=" + t.getCellName() + " expiration=" + t.getExpiration());
		} catch(AFSException e) {
			e.printStackTrace();
		}
	}

	public static void DumpFile(org.openafs.jafs.File f)
	{
		try
		{
			DumpBegin();
			f.refresh();
			Dump("File name: " + f.getPath() + ": ");
			if (f.isDirectory()) {
				Dump("This is a directory.");
			} else if (f.isLink()) {
				Dump("This is a symbolic-link.");
				Dump(" Its target is: " + f.getTarget());
				f.validate();
				if (f.isFile()) {
					Dump(" This object is now a file!");
				} else if (f.isDirectory()) {
					Dump(" This object is now a directory!");
				} else if (f.isMountPoint()) {
					Dump(" This object is now a volume mount point!");
				}
			} else if (f.isMountPoint()) {
				Dump(" This is a volume mount point.");
			} else if (f.isFile()) {
				Dump(" This is a file. Size: " + f.length());
			}
			DumpEnd();

			ACL acl = new ACL(f.getPath());
			ident++;
			DumpLn(acl.toString());
			ident--;
		} catch(AFSException e) {
			e.printStackTrace();
		}
	}

	public static void DumpCell(Cell cell)
	{
		try
		{
			DumpLn("Cell: " + cell.getName());
			ident++;
			DumpLn("MaxGroupID: " + cell.getMaxGroupID());
			DumpLn("MaxUserID: " + cell.getMaxUserID());
			ident--;
	
			//test some queries, don't write to output
			cell.getInfo();
			cell.getInfoGroups();
			cell.getInfoServers();
			cell.getInfoUsers();
		} catch(AFSException e) {
			e.printStackTrace();
		}
	}

	public static void DumpServer(Server s)
	{
		DumpLn("Server: " + s.getName());
		ident++;
		try
		{
			try //pServer'SLES9 bug:
			{
				DumpLn("BinaryRestartTime: " + s.getBinaryRestartTime());
			} catch(AFSException e) {
				e.printStackTrace();
			}
			DumpLn("TotalFreeSpace:" + s.getTotalFreeSpace());
			DumpLn("TotalSpace:" + s.getTotalSpace());
			DumpLn("TotalUsedSpace:" + s.getTotalUsedSpace());
			DumpLn("GeneralRestartTime:" + s.getGeneralRestartTime());
			DumpBegin();
			Dump("ip addresses: ");
			String[] ipAddrs = s.getIPAddresses();
			for(int i=0; i<ipAddrs.length; i++)
			{
				Dump(ipAddrs[i] + " ");
			}
			DumpEnd();
			DumpBegin();
			Dump("isFileServer: " + s.isFileServer());
			Dump(" isBadFileserver: " + s.isBadFileServer());
			Dump(" isDatabase: " + s.isDatabase());
			Dump(" isBadDatabase: " + s.isBadDatabase());
		} catch(AFSException e) {
			e.printStackTrace();
		}			
		ident--;

		try
		{
			//test some queries, don't write to output
			s.getInfo();
			try
			{
				s.getInfoKeys();
			} catch(AFSException e) {
				e.printStackTrace();
			}
			try	//is there any partitions? why parts can be null...
			{	//wrong programming concept: null instead of an empty array !!!
				s.getInfoPartitions();
			} catch(Exception e) {
				e.printStackTrace();
			}
			s.getInfoProcesses();
		} catch(AFSException e) {
			e.printStackTrace();
		}
	}

	public static void DumpVolume(Volume v)
	{
		try
		{
			DumpBegin();
			Dump("Volume name: " + v.getName());
			Dump(" ID: " + v.getID());
			DumpEnd();
		} catch(AFSException e) {
			e.printStackTrace();
		}

		ident++;
		try
		{
			DumpBegin();
			Dump("BackupID: " + v.getBackupID());
			Dump(" ReadOnlyID: " + v.getReadOnlyID());
			Dump(" ReadWriteID: " + v.getReadWriteID());
			DumpEnd();
			DumpBegin();
			Dump("LastUpdateDate: " + getGregDateStr(v.getLastUpdateDate()));
			Dump(" CreationDate: " + getGregDateStr(v.getCreationDate()));
			Dump(" AccessesSinceMidnight: " + v.getAccessesSinceMidnight());
			DumpEnd();
			DumpBegin();
			Dump("FileCount: " + v.getFileCount());
			Dump(" CurrentSize: " + v.getCurrentSize());
			Dump(" TotalFreeSpace: " + v.getTotalFreeSpace());
			DumpEnd();
			DumpBegin();
			Dump("Type: " + v.getType());
			Dump(" Disposition: " + v.getDisposition());
			DumpEnd();
			
			//test some queries, don't write to output
			v.getInfo();
		} catch(AFSException e) {
			e.printStackTrace();
		}
		ident--;
	}

	public static void DumpPartition(Partition p)
	{
		try
		{
			DumpBegin();
			Dump("Partition name: " + p.getName());
			Dump(" ID: " + p.getID());
			Dump(" DeviceName: " + p.getDeviceName());
			DumpEnd();
		} catch(AFSException e) {
			e.printStackTrace();
		}
		ident++;
		try
		{
			DumpBegin();
			Dump("TotalFreeSpace: " + p.getTotalFreeSpace());
			Dump(" TotalQuota: " + p.getTotalQuota());
			Dump(" TotalTotalSpace: " + p.getTotalSpace());
			DumpEnd();

			//test some queries, don't write to output
			p.getInfo();
			p.getInfoVolumes();
		} catch(AFSException e) {
			e.printStackTrace();
		}
		ident--;
	}

	public static void DumpGroup(Group g)
	{
		try
		{
			DumpBegin();
			Dump("Group name: " + g.getName());
			Dump(" Type: " + g.getType());
			Dump(" UID: " + g.getUID());
			DumpEnd();
			
			//test some queries, don't write to output
			g.getInfo();
		} catch(AFSException e) {
			e.printStackTrace();
		}
	}

	public static void DumpUser(User u)
	{
		DumpLn("User name: " + u.getName());
		ident++;
		try
		{

			DumpLn("EncryptionKey" + u.getEncryptionKey());
			DumpBegin();
			Dump("DaysToPasswordExpire: " + u.getDaysToPasswordExpire());
			Dump(" FailLoginCount: " + u.getFailLoginCount());
			Dump(" KeyCheckSum: " + u.getKeyCheckSum());
			DumpEnd();
			DumpBegin();
			Dump("UserExpirationDate: " + getGregDateStr(u.getUserExpirationDate()));
			Dump(" MaxTicketLifetime: " + u.getMaxTicketLifetime());
			Dump(" LockedUntilDate: " + getGregDateStr(u.getLockedUntilDate()));
			DumpEnd();

			
			//test some queries, don't write to output
			u.getInfo();
			u.getInfoGroups();
			u.getInfoGroupsOwned();
		} catch(AFSException e) {
			e.printStackTrace();
		}
		ident--;
	}

	static void DumpProcess(org.openafs.jafs.Process p)
	{
		DumpLn("Process name: " + p.getName());
		ident++;
		try
		{

			DumpBegin();
			Dump("StartTimeDate: " + getGregDateStr(p.getStartTimeDate()));
			Dump(" ExitTimeDate: " + getGregDateStr(p.getExitTimeDate()));
			DumpEnd();
			
			//test some queries, don't write to output
			p.getInfo();
		} catch(AFSException e) {
			e.printStackTrace();
		}
		ident--;
	}

	public static Token testToken(String user, String pass, String cell)
	{
		Token token = null;
		try
		{
			token = new Token(user, pass, cell);
			DumpToken(token);
			testCell(token);
		} catch(AFSException e) {
			e.printStackTrace();
		}
		return token;
	}

	public static void testFilesRecursive(File dir)
	{
		try
		{
			java.io.File fj = new java.io.File(dir.getPath());
			String[] strSubdirs = fj.list();
			for(int i=0; i<strSubdirs.length; i++)
			{
				org.openafs.jafs.File f = new org.openafs.jafs.File(
						dir.getPath() + "/" + strSubdirs[i]);
				DumpFile(f);
				f.validate();
				if (f.isDirectory() || f.isMountPoint() || f.isLink())
				{
					testFilesRecursive(dir);
				}
			}
		} catch(AFSFileException e) {
			e.printStackTrace();
		}
	}

	public static void testFiles()
	throws AFSException, AFSFileException
	{
		org.openafs.jafs.File f = new org.openafs.jafs.File(firstCellPathRW);
		DumpFile(f);
		testFilesRecursive(f);
	}
	
	public static void testCell(Token token)
	{
		Cell cell = null;
		try
		{
			cell = new Cell(token, false);

			DumpCell(cell);
		} catch(AFSException e) {
			e.printStackTrace();
		}
		if (cell==null)
			return;

		ident++;
		try
		{
			Group[]	groups = cell.getGroups();
			for(int i=0; i<groups.length; i++)
			{
				testGroup(groups[i]);
			}
	
			Server[] servers = cell.getServers();
			for(int j=0; j<servers.length; j++)
			{
				testServer(servers[j]);
			}
		} catch(AFSException e) {
			e.printStackTrace();
		}
		ident--;

		try
		{
			if (cell!=null)
				cell.close();
                } catch(AFSException e) {
                        e.printStackTrace();
                }
	}
	
	public static void testServer(Server server)
	throws AFSException
	{
		DumpServer(server);
		ident++;
		try
		{
			Partition[] parts = server.getPartitions();
			if (parts!=null)
			{
				for(int i=0; i<parts.length; i++)
				{
					testPartition(parts[i]);
				}
			}
	
			org.openafs.jafs.Process[] procs = server.getProcesses();
			if (procs!=null)
			{
				for(int i=0; i<procs.length; i++)
				{
					DumpProcess(procs[i]);
				}
			}

			DumpLn("salvage...");
			server.salvage();
			DumpLn("getLog...");
			try
			{
				server.getLog("/var/log/openafs/BosLog");
			} catch(AFSException e) {
				e.printStackTrace();
			}
			//DumpLn("stopAllProcesses...");
			//server.stopAllProcesses();
			//DumpLn("startAllProcesses...");
			//server.startAllProcesses();
			DumpLn("syncServer...");
			server.syncServer();
			DumpLn("syncVLDB...");
			server.syncVLDB();
			DumpLn("ok.");
		} catch(AFSException e) {
			e.printStackTrace();
		}
		ident--;
	}

	public static void testProcess(org.openafs.jafs.Process p)
	throws AFSException
	{
		DumpProcess(p);
	}

	public static void testPartition(Partition part)
	throws AFSException
	{
		DumpPartition(part);
		ident++;

		Volume[] vols = part.getVolumes();
		for(int i=0; i<vols.length; i++)
		{
			testVolume(vols[i]);
		}

		ident--;
	}
	
	public static void testVolume(Volume vol)
	throws AFSException
	{
		DumpVolume(vol);
	}
	
	public static void testGroup(Group group)
	throws AFSException
	{
		DumpGroup(group);
		ident++;

		User[] users = group.getMembers();
		for(int i=0; i<users.length; i++)
		{
			testUser(users[i]);
		}

		ident--;
	}
	
	public static void testUser(User user)
	throws AFSException
	{
		DumpUser(user);
	}

	public static void testNewVolume(String cellName, String userName, String passString)
	{
		if (firstCellPathRW==null)
		{
			System.err.println("testNewVolume cannot be executed (null args).");
			return;
		}

		String volName = "myTestJafsVolume92834";
		String mpName = firstCellPathRW + "/" + volName;
		try
		{
			Token token = new Token(cellName, userName, passString);
			Cell cell = new Cell(token, false);
			Server[] servers = cell.getServers();
			if (servers.length==0)
			{
				System.err.println("Error: failed to run\"testNewVolume\"!");
				return;
			}
			Partition firstPart = null;
			int i;
			for(i=0; i<servers.length; i++)
			{
				Partition[] parts = servers[i].getPartitions();
				if (parts==null)
					continue;

				if (parts.length!=0)
				{
					firstPart = parts[0];
					break;
				}
			}
			if (firstPart==null)
			{
				System.err.println("Error: failed to find any partition on any server - \"testNewVolume\" couldn't be tested!");
				return;
			}

			Volume v = new Volume(volName, firstPart);
			DumpLn("Creating a new volume " + volName + " ...");
			v.create(0);

			v.lock();
			v.unlock();

			volName = "myTestJafsVolume2389";
			DumpLn("Renaming volume to " + volName + " ...");
			v.rename(volName);

			DumpLn("Creating an rw mount point " + mpName + " ...");
			v.createMountPoint(mpName, true);

			DumpLn("Deleting mount point...");
			java.lang.Runtime.getRuntime().exec("fs rmmount " + mpName);
			
			DumpLn("Creating RO...");
			Volume volRO = v.createReadOnly(firstPart);

			DumpLn("Creating backup...");
			v.createBackup();

			DumpLn("Releaseing volume...");
			v.release();

			//v.moveTo();	//needs a more careful test env

			DumpLn("Salvaging volume...");
			v.salvage();

			DumpLn("Deleting volume...");
			v.delete();

			DumpLn("Deleting RO site...");
			volRO.delete();

			DumpLn("OK.");
		} catch(Exception e) {
			e.printStackTrace();
		}
	}
	
	public static void main(String[] args)
	{

		try
		{
			if (args.length<4)
			{
				System.err.println("testAFS <user> <pass> <cell_name> <# of cycles>");
				return;
			}

			Class.forName("org.openafs.jafs.Token");	//initialization...
			System.out.println("Java interface version: " + VersionInfo.getVersionOfJavaInterface());
			System.out.println("Library version: " + VersionInfo.getVersionOfLibrary());
			System.out.println("Build info: " + VersionInfo.getBuildInfo());

			//first test whether token is valid
			//and load libraries with it
			Token t0 = new Token(args[0], args[1], args[2]);
			t0.close();

			System.out.print("Starting another tester thread...");
			testAFS	ta = new testAFS();
			TesterThread tt = ta.new TesterThread(args[0], args[1], args[2]);
			Thread tTest = new Thread(tt);
			tTest.start();
			System.out.println("started.");

			firstCellPathRW = "/afs/." + args[2];
			int numOfCycles = Integer.parseInt(args[3]);
			for(int i=0; i<numOfCycles || numOfCycles==0; i++)
			{
				testToken(args[0], args[1], args[2]);

				testFiles();

				testNewVolume(args[0], args[1], args[2]);

				System.out.print("ACL excercising...");
				allowDump = false;
				for(int j=0; j<100; j++)
				{
					testFiles();
					System.out.print(".");
				}
				System.out.println();
				allowDump = true;

				AFSException afs = new AFSException(1);

				System.out.println("cycle #" + (i+1) + "/" + numOfCycles + " done.");
			}

			tt.finish();
			System.out.println("All done.");
		} catch (Exception e) {
			e.printStackTrace();
		}
	}
}
