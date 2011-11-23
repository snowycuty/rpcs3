#include "stdafx.h"
#include "System.h"
#include "Emu/Memory/Memory.h"
#include "Emu/ElfLoader.h"
#include "Ini.h"

#include "Emu/Cell/PPU.h"
#include "Emu/Cell/SPU.h"

Emulator::Emulator()
{
	m_status = Stoped;
	m_mode = 0;
}

void Emulator::SetSelf(wxString self_patch)
{
	Loader.SetElf(self_patch);
	IsSelf = true;
}

void Emulator::SetElf(wxString elf_patch)
{
	Loader.SetElf(elf_patch);
	IsSelf = false;
}

CPUThread& Emulator::AddThread(bool isSPU)
{
	CPUThread* cpu; 
	if(isSPU) cpu = new SPUThread(GetCPU().GetCount());
	else cpu = new PPUThread(GetCPU().GetCount());
	GetCPU().Add(cpu);
	return *cpu;
}

void Emulator::RemoveThread(const u8 core)
{
	ConLog.Warning("Remove Thread %d of %d", core+1, GetCPU().GetCount());
	if(core >= GetCPU().GetCount()) return;
	GetCPU().RemoveFAt(core);
	
	for(uint i=core; i<GetCPU().GetCount(); ++i)
	{
		GetCPU().Get(i).core--;
	}

	CheckStatus();
}

void Emulator::CheckStatus()
{
	if(GetCPU().GetCount() == 0)
	{
		Stop();
		return;	
	}

	bool IsAllPaused = true;
	for(uint i=0; i<GetCPU().GetCount(); ++i)
	{
		if(!GetCPU().Get(i).IsPaused())
		{
			IsAllPaused = false;
			break;
		}
	}
	if(IsAllPaused)
	{
		Pause();
		return;
	}

	bool IsAllStoped = true;
	for(uint i=0; i<GetCPU().GetCount(); ++i)
	{
		if(!GetCPU().Get(i).IsStoped())
		{
			IsAllStoped = false;
			break;
		}
	}
	if(IsAllStoped) Stop();
}

void Emulator::Run()
{
	if(IsRunned()) Stop();
	if(IsPaused())
	{
		Resume();
		return;
	}

	//ConLog.Write("run...");
	m_status = Runned;

	Memory.Init();

	if(!Loader.Load(IsSelf))
	{
		Memory.Close();
		return;
	}
	
	CPUThread& cpu_thread = AddThread(Loader.isSPU);
	if(Ini.Emu.m_DecoderMode.GetValue() == 1)
	{
		cpu_thread.SetPc(Loader.entry - 4);
	}
	else
	{
		cpu_thread.SetPc(Loader.entry);
	}

	if(m_memory_viewer && m_memory_viewer->exit) safe_delete(m_memory_viewer);
	if(!m_memory_viewer) m_memory_viewer = new MemoryViewerPanel(NULL);

	m_memory_viewer->SetPC(cpu_thread.PC);
	m_memory_viewer->Show();
	m_memory_viewer->ShowPC();

	cpu_thread.Run();

	wxGetApp().m_MainFrame->UpdateUI();
}

void Emulator::Pause()
{
	if(!IsRunned()) return;
	//ConLog.Write("pause...");

	m_status = Paused;
	wxGetApp().m_MainFrame->UpdateUI();
}

void Emulator::Resume()
{
	if(!IsPaused()) return;
	//ConLog.Write("resume...");

	m_status = Runned;
	wxGetApp().m_MainFrame->UpdateUI();

	for(uint i=0; i<GetCPU().GetCount(); ++i)
	{
		GetCPU().Get(i).SysResume();
	}

	CheckStatus();
}

void Emulator::Stop()
{
	if(IsStoped()) return;
	//ConLog.Write("shutdown...");

	m_status = Stoped;
	wxGetApp().m_MainFrame->UpdateUI();

	for(uint i=0; i<GetCPU().GetCount(); ++i)
	{
		GetCPU().Get(i).Close();
	}

	GetCPU().Clear();

	Memory.Close();
	CurGameInfo.Reset();

	if(m_memory_viewer && !m_memory_viewer->exit) m_memory_viewer->Hide();
}

Emulator Emu;