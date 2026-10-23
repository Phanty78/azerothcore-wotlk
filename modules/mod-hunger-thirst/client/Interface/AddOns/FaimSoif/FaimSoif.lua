--[[--------------------------------------------------------------------------
    FaimSoif - jauges de faim et de soif (client 3.3.5a, Interface 30300).

    Protocole serveur, prefixe d'addon "FS" :
        FS<TAB>satiety<TAB>hydration<TAB>mode<TAB>lethal<TAB>loaded
          <TAB>persistence<TAB>satBand<TAB>hydBand

    mode : 0 = desactive, 1 = avis, 2 = survie
    band : 0 = normal, 1 = avertissement, 2 = critique, 3 = vide

    Le prefixe est transmis separement par CHAT_MSG_ADDON ; le jeton "FS"
    place en tete du message est accepte puis ignore.
----------------------------------------------------------------------------]]

local ADDON_PREFIX = "FS"
local MAX_VALUE = 100
local DEFAULT_POSITION = { point = "CENTER", relativePoint = "CENTER", x = -160, y = -140 }

local MODE_LABELS = {
    [0] = "Désactivé",
    [1] = "Avis",
    [2] = "Survie",
}

-- Couleurs de seuil : normal, jaune, orange, rouge.
local BAND_COLORS = {
    [0] = { 0.35, 1.00, 0.40 },
    [1] = { 1.00, 0.90, 0.25 },
    [2] = { 1.00, 0.55, 0.10 },
    [3] = { 1.00, 0.20, 0.20 },
}

-- Couleurs d'identite des deux jauges, utilisees tant que la bande est normale.
local BAR_COLORS = {
    satiety   = { 1.00, 0.62, 0.12 },
    hydration = { 0.20, 0.58, 1.00 },
}

local BAND_LABELS = {
    satiety   = { [0] = "Normal", [1] = "Faim", [2] = "Famine", [3] = "Affamé" },
    hydration = { [0] = "Normal", [1] = "Soif", [2] = "Critique", [3] = "Déshydraté" },
}

local State = {
    satiety = MAX_VALUE,
    hydration = MAX_VALUE,
    mode = 0,
    lethal = false,
    loaded = false,
    persistence = false,
    satietyBand = 0,
    hydrationBand = 0,
}

if type(FaimSoifDB) ~= "table" then
    FaimSoifDB = {}
end

local function Clamp(value)
    if value < 0 then
        return 0
    end
    if value > MAX_VALUE then
        return MAX_VALUE
    end
    return value
end

local function NormalizeBand(band)
    band = math.floor(tonumber(band) or 0)
    if band < 0 or band > 3 then
        return 0
    end
    return band
end

local function NormalizeMode(mode)
    mode = math.floor(tonumber(mode) or 0)
    if mode < 0 or mode > 2 then
        return 0
    end
    return mode
end

local function BandColor(band)
    local color = BAND_COLORS[band] or BAND_COLORS[0]
    return color[1], color[2], color[3]
end

local function BarFillColor(key, band)
    if band == 0 then
        local color = BAR_COLORS[key]
        return color[1], color[2], color[3]
    end
    return BandColor(band)
end

--------------------------------------------------------------------------------
-- Cadre
--------------------------------------------------------------------------------

local Frame = CreateFrame("Frame", "FaimSoifFrame", UIParent)
Frame:SetWidth(240)
Frame:SetHeight(142)
Frame:SetFrameStrata("MEDIUM")
Frame:SetClampedToScreen(true)
Frame:SetMovable(true)
Frame:EnableMouse(true)
Frame:RegisterForDrag("LeftButton")
Frame:SetBackdrop({
    bgFile = "Interface\\ChatFrame\\ChatFrameBackground",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true,
    tileSize = 16,
    edgeSize = 16,
    insets = { left = 4, right = 4, top = 4, bottom = 4 },
})
Frame:SetBackdropColor(0.05, 0.05, 0.05, 0.78)
Frame:SetBackdropBorderColor(0.55, 0.45, 0.25, 1)

local Title = Frame:CreateFontString(nil, "OVERLAY", "GameFontNormal")
Title:SetPoint("TOP", Frame, "TOP", 0, -7)
Title:SetText("Faim & Soif")
Title:SetTextColor(1.00, 0.82, 0.25)

local function CreateGauge(parent, name, key)
    local gauge = { name = name, key = key }

    gauge.label = parent:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    gauge.label:SetWidth(168)
    gauge.label:SetJustifyH("LEFT")
    gauge.label:SetText(name)

    gauge.value = parent:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
    gauge.value:SetJustifyH("RIGHT")
    gauge.value:SetText("--")

    gauge.bar = CreateFrame("StatusBar", nil, parent)
    gauge.bar:SetHeight(13)
    gauge.bar:SetMinMaxValues(0, MAX_VALUE)
    gauge.bar:SetValue(0)
    gauge.bar:SetStatusBarTexture("Interface\\TargetingFrame\\UI-StatusBar")
    gauge.bar:SetBackdrop({
        bgFile = "Interface\\ChatFrame\\ChatFrameBackground",
        edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
        tile = true,
        tileSize = 8,
        edgeSize = 10,
        insets = { left = 1, right = 1, top = 1, bottom = 1 },
    })
    gauge.bar:SetBackdropColor(0, 0, 0, 0.60)
    gauge.bar:SetBackdropBorderColor(0.30, 0.30, 0.30, 1)

    return gauge
end

local SatietyGauge = CreateGauge(Frame, "Satiété", "satiety")
SatietyGauge.label:SetPoint("TOPLEFT", Frame, "TOPLEFT", 12, -26)
SatietyGauge.value:SetPoint("TOPRIGHT", Frame, "TOPRIGHT", -12, -26)
SatietyGauge.bar:SetPoint("TOPLEFT", SatietyGauge.label, "BOTTOMLEFT", 0, -3)
SatietyGauge.bar:SetPoint("TOPRIGHT", SatietyGauge.value, "BOTTOMRIGHT", 0, -3)

local HydrationGauge = CreateGauge(Frame, "Hydratation", "hydration")
HydrationGauge.label:SetPoint("TOPLEFT", SatietyGauge.bar, "BOTTOMLEFT", 0, -10)
HydrationGauge.value:SetPoint("TOPRIGHT", SatietyGauge.bar, "BOTTOMRIGHT", 0, -10)
HydrationGauge.bar:SetPoint("TOPLEFT", HydrationGauge.label, "BOTTOMLEFT", 0, -3)
HydrationGauge.bar:SetPoint("TOPRIGHT", HydrationGauge.value, "BOTTOMRIGHT", 0, -3)

local Status = Frame:CreateFontString(nil, "OVERLAY", "GameFontHighlightSmall")
Status:SetPoint("TOPLEFT", HydrationGauge.bar, "BOTTOMLEFT", 0, -10)
Status:SetPoint("TOPRIGHT", HydrationGauge.bar, "BOTTOMRIGHT", 0, -10)
Status:SetHeight(30)
Status:SetJustifyH("LEFT")
Status:SetJustifyV("TOP")

--------------------------------------------------------------------------------
-- Mise a jour de l'affichage
--------------------------------------------------------------------------------

local function UpdateGauge(gauge, value, band)
    local fillR, fillG, fillB = BarFillColor(gauge.key, band)
    gauge.bar:SetStatusBarColor(fillR, fillG, fillB)

    if State.loaded then
        local current = Clamp(value)
        gauge.bar:SetValue(current)
        gauge.value:SetText(string.format("%.0f %%", current))
        gauge.label:SetText(string.format("%s (%s)", gauge.name, BAND_LABELS[gauge.key][band]))

        local r, g, b = BandColor(band)
        gauge.label:SetTextColor(r, g, b)
        gauge.value:SetTextColor(r, g, b)
    else
        gauge.bar:SetValue(0)
        gauge.value:SetText("--")
        gauge.value:SetTextColor(0.65, 0.65, 0.65)
        gauge.label:SetText(gauge.name)
        gauge.label:SetTextColor(0.65, 0.65, 0.65)
    end
end

local function UpdateStatus()
    if not State.loaded then
        Status:SetText("|cffffd200Chargement...|r")
        return
    end

    local modeText = MODE_LABELS[State.mode] or MODE_LABELS[0]
    if State.mode == 0 then
        modeText = "|cff9d9d9d" .. modeText .. "|r"
    end

    local text = "Mode : " .. modeText
    if State.lethal then
        text = text .. " · |cffff4040Létal|r"
    end
    if not State.persistence then
        text = text .. " · |cffff9d00Non persisté|r"
    end
    Status:SetText(text)
end

local function Update()
    UpdateGauge(SatietyGauge, State.satiety, State.satietyBand)
    UpdateGauge(HydrationGauge, State.hydration, State.hydrationBand)
    UpdateStatus()
end

--------------------------------------------------------------------------------
-- Position (SavedVariables)
--------------------------------------------------------------------------------

local function SavePosition()
    local point, _, relativePoint, x, y = Frame:GetPoint(1)
    if type(point) ~= "string" or type(relativePoint) ~= "string" then
        return
    end

    FaimSoifDB.point = point
    FaimSoifDB.relativePoint = relativePoint
    FaimSoifDB.x = x
    FaimSoifDB.y = y
end

local function ApplySavedPosition()
    Frame:ClearAllPoints()

    local point = FaimSoifDB.point
    local relativePoint = FaimSoifDB.relativePoint
    if type(point) == "string" and type(relativePoint) == "string" then
        Frame:SetPoint(point, UIParent, relativePoint, tonumber(FaimSoifDB.x) or 0, tonumber(FaimSoifDB.y) or 0)
    else
        Frame:SetPoint(DEFAULT_POSITION.point, UIParent, DEFAULT_POSITION.relativePoint, DEFAULT_POSITION.x,
            DEFAULT_POSITION.y)
    end
end

local function ResetPosition()
    FaimSoifDB.point = nil
    FaimSoifDB.relativePoint = nil
    FaimSoifDB.x = nil
    FaimSoifDB.y = nil
    ApplySavedPosition()
end

Frame:SetScript("OnDragStart", function(self)
    self:StartMoving()
end)

Frame:SetScript("OnDragStop", function(self)
    self:StopMovingOrSizing()
    SavePosition()
end)

Frame:SetScript("OnShow", function()
    Update()
end)

--------------------------------------------------------------------------------
-- Protocole
--------------------------------------------------------------------------------

local function ParsePayload(text)
    if type(text) ~= "string" then
        return nil
    end

    local fields = { strsplit("\t", text) }
    if fields[1] == ADDON_PREFIX then
        table.remove(fields, 1)
    end

    if #fields < 8 then
        return nil
    end

    local satiety = tonumber(fields[1])
    local hydration = tonumber(fields[2])
    if not satiety or not hydration then
        return nil
    end

    return {
        satiety = Clamp(satiety),
        hydration = Clamp(hydration),
        mode = NormalizeMode(fields[3]),
        lethal = (tonumber(fields[4]) or 0) ~= 0,
        loaded = (tonumber(fields[5]) or 0) ~= 0,
        persistence = (tonumber(fields[6]) or 0) ~= 0,
        satietyBand = NormalizeBand(fields[7]),
        hydrationBand = NormalizeBand(fields[8]),
    }
end

local function ApplyPayload(payload)
    State.satiety = payload.satiety
    State.hydration = payload.hydration
    State.mode = payload.mode
    State.lethal = payload.lethal
    State.loaded = payload.loaded
    State.persistence = payload.persistence
    State.satietyBand = payload.satietyBand
    State.hydrationBand = payload.hydrationBand
    Update()
end

local function OnAddonMessage(prefix, text)
    if prefix ~= ADDON_PREFIX then
        return
    end

    local payload = ParsePayload(text)
    if payload then
        ApplyPayload(payload)
    end
end

--------------------------------------------------------------------------------
-- Evenements
--------------------------------------------------------------------------------

local EventFrame = CreateFrame("Frame")
EventFrame:RegisterEvent("CHAT_MSG_ADDON")
EventFrame:RegisterEvent("PLAYER_LOGIN")
EventFrame:SetScript("OnEvent", function(_, event, ...)
    if event == "CHAT_MSG_ADDON" then
        local prefix, text = ...
        OnAddonMessage(prefix, text)
    elseif event == "PLAYER_LOGIN" then
        if RegisterAddonMessagePrefix then
            RegisterAddonMessagePrefix(ADDON_PREFIX)
        end
        Update()
    end
end)

if RegisterAddonMessagePrefix then
    RegisterAddonMessagePrefix(ADDON_PREFIX)
end

--------------------------------------------------------------------------------
-- Commande utilisateur
--------------------------------------------------------------------------------

-- "/faim" est une commande purement cliente : la commande serveur equivalente
-- s'ecrit ".faim" et n'entre donc pas en conflit.
SLASH_FAIMSOIF1 = "/faim"
SlashCmdList["FAIMSOIF"] = function(message)
    local command = string.lower(message or "")
    command = string.match(command, "^%s*(.-)%s*$") or ""

    if command == "reset" then
        ResetPosition()
        Frame:Show()
        Update()
    elseif Frame:IsShown() then
        Frame:Hide()
    else
        Frame:Show()
    end
end

ApplySavedPosition()
Update()
