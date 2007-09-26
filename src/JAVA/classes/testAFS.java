import java.io.File;
import java.util.GregorianCalendar;

import org.openafs.jafs.*;



/**
 * @version 	1.0
 * @author
 */
public class testAFS
{
	private static boolean dflag = false;
	private static boolean something_failed = false;

	public class TesterThread implements Runnable
	{
		private String user = null;
		private String pass = null;
		private String cell = null;
		private String rwpath = null;
		private boolean letItRun = true;

		public TesterThread(String user, String pass, String cell, String rwpath)
		{
			this.user = user;
			this.pass = pass;
			this.cell = cell;
			this.rwpath = rwpath;
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
						ACL acl = new ACL(rwpath, true);
					}

					c.close();
				} catch(Exception e) {
					something_failed = true;
					e.printStackTrace();
					letItRun = false;
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
	throws AFSException
	{
		try
		{
			DumpLn("Token: user=" + t.getUsername() +
					" cell=" + t.getCellName() + " expiration=" + t.getExpiration());
		} catch(AFSException e) {
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
	}

	public static void DumpFile(org.openafs.jafs.File f)
	throws AFSException
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
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
	}

	public static void DumpCell(Cell cell)
	throws AFSException
	{
		try
		{
			DumpLn("Cell: " + cell.getName());
			ident++;
			DumpLn("MaxGroupID: " + cell.getMaxGroupID());
			DumpLn("MaxUserID: " + cell.getMaxUserID());
			ident--;
	
			//test some queries, don't write to output
			if (dflag) System.out.println("DumpCell/getInfo");
			cell.getInfo();
			if (dflag) System.out.println("DumpCell/getInfoGroups");
			cell.getInfoGroups();
			if (dflag) System.out.println("DumpCell/getInfoServers");
			cell.getInfoServers();
			if (dflag) System.out.println("DumpCell/getInfoUsers");
			cell.getInfoUsers();
		} catch(AFSException e) {
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
	}

	public static void DumpServer(Server s)
	throws AFSException, Exception
	{
		DumpLn("Server: " + s.getName());
		ident++;
		try
		{
			try //pServer'SLES9 bug:
			{
				DumpLn("BinaryRestartTime: " + s.getBinaryRestartTime());
			} catch(AFSException e) {
				something_failed = true;
//				e.printStackTrace();
				throw(e);
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
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}			
		ident--;

		try
		{
			//test some queries, don't write to output
			if (dflag) System.out.println("DumpServer/getInfoKeys");
			s.getInfo();
			try
			{
				s.getInfoKeys();
			} catch(AFSException e) {
				something_failed = true;
//				e.printStackTrace();
				throw(e);
			}
			if (dflag) System.out.println("DumpServer/getInfoPartitions");
			try	//is there any partitions? why parts can be null...
			{	//wrong programming concept: null instead of an empty array !!!
				s.getInfoPartitions();
			} catch(Exception e) {
				something_failed = true;
//				e.printStackTrace();
				throw(e);
			}
			if (dflag) System.out.println("DumpServer/getInfoProcesses");
			s.getInfoProcesses();
		} catch(AFSException e) {
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
	}

	public static void DumpVolume(Volume v)
	throws AFSException
	{
		try
		{
			DumpBegin();
			Dump("Volume name: " + v.getName());
			Dump(" ID: " + v.getID());
			DumpEnd();
		} catch(AFSException e) {
			something_failed = true;
//			e.printStackTrace();
			throw(e);
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
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
		ident--;
	}

	public static void DumpPartition(Partition p)
	throws AFSException
	{
		try
		{
			DumpBegin();
			Dump("Partition name: " + p.getName());
			Dump(" ID: " + p.getID());
			Dump(" DeviceName: " + p.getDeviceName());
			DumpEnd();
		} catch(AFSException e) {
			something_failed = true;
//			e.printStackTrace();
			throw(e);
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
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
		ident--;
	}

	public static void DumpGroup(Group g)
	throws AFSException
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
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
	}

	public static void DumpUser(User u)
	throws AFSException
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
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
		ident--;
	}

	static void DumpProcess(org.openafs.jafs.Process p)
	throws AFSException
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
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
		ident--;
	}

	public static Token testToken(String user, String pass, String cell)
	throws AFSException, Exception
	{
		Token token = null;

		if (dflag) System.out.println("testToken");
		try
		{
			token = new Token(user, pass, cell);
			DumpToken(token);
			testCell(token);
		} catch(AFSException e) {
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
		return token;
	}

	public static void testFilesRecursive(File dir)
	throws AFSException, AFSFileException
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
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
	}

	public static void testFiles()
	throws AFSException, AFSFileException
	{
		if (dflag) System.out.println("testFiles");
		org.openafs.jafs.File f = new org.openafs.jafs.File(firstCellPathRW);
		DumpFile(f);
		testFilesRecursive(f);
	}
	
	public static void testCell(Token token)
	throws AFSException, Exception
	{
		Cell cell = null;
		if (dflag) System.out.println("testCell");
		try
		{
			cell = new Cell(token, false);

			DumpCell(cell);
		} catch(AFSException e) {
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
		if (cell==null)
			return;

		ident++;
		try
		{
			if (dflag) System.out.println("testCell/testGroup");
			Group[]	groups = cell.getGroups();
			for(int i=0; i<groups.length; i++)
			{
				testGroup(groups[i]);
			}
	
			if (dflag) System.out.println("testCell/testServer");
			Server[] servers = cell.getServers();
			for(int j=0; j<servers.length; j++)
			{
				testServer(servers[j]);
			}
		} catch(AFSException e) {
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
		ident--;

		try
		{
			if (cell!=null)
				cell.close();
                } catch(AFSException e) {
			something_failed = true;
//			e.printStackTrace();
			throw(e);
                }
	}
	
	public static void testServer(Server server)
	throws AFSException, Exception
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

			DumpLn("salvage "+server.getName()+"...");
			server.salvage();
			DumpLn("getLog "+server.getName()+"...");
			try
			{
				server.getLog("BosLog");
			} catch(AFSException e) {
				something_failed = true;
//				e.printStackTrace();
				throw(e);
			}
			//DumpLn("stopAllProcesses...");
			//server.stopAllProcesses();
			//DumpLn("startAllProcesses...");
			//server.startAllProcesses();
			DumpLn("syncServer "+server.getName()+"...");
			server.syncServer();
			DumpLn("syncVLDB "+server.getName()+"...");
			server.syncVLDB();
			DumpLn("ok.");
		} catch(AFSException e) {
			something_failed = true;
//			e.printStackTrace();
			throw(e);
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
		if (dflag) System.out.println("testPartition");
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
	throws AFSException, Exception
	{
		if (dflag) System.out.println("testNewVolume");
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
			something_failed = true;
//			e.printStackTrace();
			throw(e);
		}
	}
	
	public static void main(String[] args)
	{
		String user = null, pass = null, cell_name = null;
		int numOfCycles = 1;
		String Usage = "Usage: testAFS <user> <pass> <cell_name> <# of cycles>";
		int argc, k = 0;

		for (argc = 0; argc < args.length; ++argc)
		{
			if (!args[argc].startsWith("-"))
				break;
			char cw[] = args[argc].toCharArray();
			for (int i = 0; i < cw.length; ++i) switch(cw[i]) {
			case '-':
				break;
			case 'f':
				if (argc >= args.length) break;
				firstCellPathRW = args[++argc];
				break;
			case 'd':
				dflag = true;
				break;
			default:
				System.err.println("Bad switch " + cw[i]);
				System.err.println(Usage);
				return;
			}
		}
		for (; argc < args.length; ++argc)
		{
			switch(k++) {
			case 0:
				user = args[argc];
				break;
			case 1:
				pass = args[argc];
				break;
			case 2:
				cell_name = args[argc];
				break;
			case 3:
				numOfCycles = Integer.parseInt(args[argc]);
				break;
			default:
				System.err.println("Too many bare arguments");
				System.err.println(Usage);
				return;
			}
		}
		if (k < 3)
		{
			System.err.println("Too few bare arguments");
			System.err.println(Usage);
			return;
		}
		if (firstCellPathRW == null)
			firstCellPathRW = "/afs/." + args[2];

		TesterThread tt = null;
		try
		{
			Class.forName("org.openafs.jafs.Token");	//initialization...
			System.out.println("Java interface version: " + VersionInfo.getVersionOfJavaInterface());
			System.out.println("Library version: " + VersionInfo.getVersionOfLibrary());
			System.out.println("Build info: " + VersionInfo.getBuildInfo());

			//first test whether token is valid
			//and load libraries with it
			Token t0 = new Token(user, pass, cell_name);
			t0.close();

			System.out.print("Starting another tester thread...");
			testAFS	ta = new testAFS();
			tt = ta.new TesterThread(user, pass, cell_name, firstCellPathRW);
			Thread tTest = new Thread(tt);
			tTest.start();
			System.out.println("started.");

			for(int i=0; i<numOfCycles || numOfCycles==0; i++)
			{
				testToken(user, pass, cell_name);

				testFiles();

				testNewVolume(user, pass, cell_name);

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
		} catch (Exception e) {
			something_failed = true;
			e.printStackTrace();
			System.out.println("Bailing - fatal error.");
		} finally {
			if (tt != null) tt.finish();
			if (!something_failed)
				System.out.println("All done.");
		}
	}
}
