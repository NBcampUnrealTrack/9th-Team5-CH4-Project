using System;
using System.Diagnostics;
using System.IO;

internal static class RoomMasterLauncher
{
    private static int Main()
    {
        try
        {
            string directory = AppDomain.CurrentDomain.BaseDirectory;
            string project = Path.GetFullPath(Path.Combine(directory, "..", "DeepRaiders.uproject"));
            string engine = File.ReadAllText(Path.Combine(directory, "EnginePath.txt")).Trim();
            string executable = Path.Combine(engine, "Engine", "Binaries", "Win64", "UnrealEditor-Cmd.exe");
            if (!File.Exists(project) || !File.Exists(executable))
            {
                throw new FileNotFoundException("Check EnginePath.txt and the project location.");
            }

            // Master console stays attached so startup errors remain visible.
            ProcessStartInfo start = new ProcessStartInfo(executable);
            start.Arguments = "\"" + project + "\" -run=RoomMaster -unattended -nullrhi"
                + " -stdout -FullStdOutLogOutput";
            start.WorkingDirectory = Path.GetDirectoryName(project);
            start.UseShellExecute = false;
            using (Process master = Process.Start(start))
            {
                master.WaitForExit();
                if (master.ExitCode == 0)
                {
                    return 0;
                }
                Console.Error.WriteLine("RoomMaster exited: " + master.ExitCode);
            }
        }
        catch (Exception error)
        {
            Console.Error.WriteLine(error.Message);
        }
        Console.WriteLine("Press Enter to close.");
        Console.ReadLine();
        return 1;
    }
}
